#!/usr/bin/env python3
"""Local-only end-to-end checks. Isolated DB, no passwords/tokens in output."""
import argparse,collections,concurrent.futures,datetime,json,pathlib,secrets,socket,sqlite3,subprocess,tempfile,threading,time,urllib.error,urllib.request
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);a=p.parse_args();build=a.build.resolve()
counts=collections.Counter();checks=0
# Loopback verification must not be forwarded through machine proxy settings.
opener=urllib.request.build_opener(urllib.request.ProxyHandler({}))
with tempfile.TemporaryDirectory(prefix='seat-http-') as directory:
    root=pathlib.Path(directory);database=root/'test.db';password=secrets.token_hex(16)
    cli=subprocess.run([str(build/'seat_cli'),'--db',str(database)],input=f'bootstrap\n{password}\nquit\n',text=True,capture_output=True,timeout=10)
    assert cli.returncode==0 and 'error' not in cli.stdout and password not in cli.stdout,cli.stdout
    with socket.socket() as sock:sock.bind(('127.0.0.1',0));port=sock.getsockname()[1]
    base=f'http://127.0.0.1:{port}';process=None
    log=open(root/'server.log','w+')
    def request(method,path,data=None,token=None):
        headers={}
        if token:headers['Authorization']='Bearer '+token
        if data is not None:headers['Content-Type']='application/json'
        req=urllib.request.Request(base+path,data=None if data is None else json.dumps(data).encode(),headers=headers,method=method)
        try:
            with opener.open(req,timeout=5) as r:return r.status,json.load(r)
        except urllib.error.HTTPError as e:return e.code,json.load(e)
    def expect(method,path,data=None,token=None,status=200,code=None):
        global checks
        actual,value=request(method,path,data,token);checks+=1;counts[str(actual)]+=1
        assert actual==status,(method,path,actual,value,status)
        if code:assert value['error']['code']==code,value
        return value
    def start():
        global process
        process=subprocess.Popen([str(build/'seat_http'),'--db',str(database),'--port',str(port)],stdout=log,stderr=log)
        deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            if process.poll() is not None:log.seek(0);raise AssertionError(log.read())
            try:
                if request('GET','/health')[0]==200:return
            except (urllib.error.URLError,TimeoutError,ConnectionError):pass
            time.sleep(.03)
        raise AssertionError('server startup timeout')
    def stop():
        if process and process.poll() is None:
            process.terminate()
            try:process.wait(timeout=10)
            except subprocess.TimeoutExpired:process.kill();process.wait();raise
    try:
        start()
        day=datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).date()+datetime.timedelta(days=7)
        while day.weekday()>4:day+=datetime.timedelta(days=1)
        date=day.isoformat()
        admin=expect('POST','/api/login',{'name':'admin','password':password})['token']
        expect('POST','/api/login',{'name':'admin','password':'wrong'},status=401,code='unauthorized')
        expect('GET','/api/me',status=401,code='unauthorized')
        expect('POST','/api/login',[],status=400,code='invalid_input')
        expect('GET','/unknown',status=404,code='http_error')
        tokens=[]
        for i in range(20):
            expect('POST','/api/users',{'name':f's{i}','password':password,'role':0},admin)
            tokens.append(expect('POST','/api/login',{'name':f's{i}','password':password})['token'])
        expect('POST','/api/users',{'name':'teacher','password':password,'role':1},admin)
        teacher=expect('POST','/api/login',{'name':'teacher','password':password})['token']
        expect('POST','/api/rooms',{'name':'concurrency','capacity':10},admin)
        room=expect('POST','/api/rooms',{'name':'workflow','capacity':2},admin)['id']
        expect('POST','/api/rooms',{'name':'forbidden','capacity':1},tokens[0],403,'forbidden')
        data={'request_id':'flow','room_id':room,'date':date,'slot':1}
        r=expect('POST','/api/reservations',data,tokens[0]);assert r['status']=='Pending'
        assert expect('POST','/api/reservations',data,tokens[0])==r
        expect('POST','/api/reservations',{**data,'room_id':1},tokens[0],409,'conflict')
        expect('POST','/api/reservations',{**data,'request_id':'duplicate','room_id':1},tokens[0],409,'conflict')
        expect('POST','/api/reservations',{**data,'student_id':999},tokens[0],400,'invalid_input')
        expect('GET',f'/api/reservations/detail?id={r["id"]}',token=tokens[1],status=403,code='forbidden')
        assert expect('GET','/api/reservations',token=tokens[1])==[]
        assert len(expect('GET','/api/reservations',token=teacher))==1
        expect('POST','/api/rooms/capacity',{'room_id':room,'date':date,'slot':1,'capacity':0},admin,409,'conflict')
        review={'id':r['id'],'expected':'Pending','target':'Approved'}
        expect('POST','/api/reservations/review',review,tokens[0],403,'forbidden')
        expect('POST','/api/reservations/cancel',{'id':r['id'],'expected':'Pending'},teacher,403,'forbidden')
        expect('POST','/api/reservations/review',review,teacher)
        expect('POST','/api/reservations/review',review,teacher,409,'conflict')
        expect('POST','/api/reservations/cancel',{'id':r['id'],'expected':'Pending'},tokens[0],409,'conflict')
        cancel={'id':r['id'],'expected':'Approved'}
        expect('POST','/api/reservations/cancel',cancel,tokens[0])
        expect('POST','/api/reservations/cancel',cancel,tokens[0],409,'conflict')
        assert expect('POST','/api/reservations',data,tokens[0])==r
        with sqlite3.connect(database) as db:
            db.execute('BEGIN IMMEDIATE')
            expect('POST','/api/reservations',{**data,'request_id':'busy'},tokens[0],503,'database_busy')
            db.rollback()
        busy=expect('POST','/api/reservations',{**data,'request_id':'busy'},tokens[0])
        expect('POST','/api/reservations/review',{'id':busy['id'],'expected':'Pending','target':'Rejected'},teacher)
        # Drop the client socket after sending a complete request; server callbacks
        # must retain valid owned state even if the peer disappears.
        disconnected={**data,'request_id':'disconnected'}
        payload=json.dumps(disconnected).encode()
        wire=(f'POST /api/reservations HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: Bearer {tokens[0]}\r\nContent-Type: application/json\r\nContent-Length: {len(payload)}\r\nConnection: close\r\n\r\n').encode()+payload
        with socket.create_connection(('127.0.0.1',port),timeout=5) as sock:
            sock.sendall(wire)
        recovered=expect('POST','/api/reservations',disconnected,tokens[0])
        expect('POST','/api/reservations/cancel',{'id':recovered['id'],'expected':'Pending'},tokens[0])
        # All clients rendezvous before issuing HTTP; retry only classified busy/overload.
        gate=threading.Barrier(20)
        def race(i):
            gate.wait(timeout=10);busy_attempts=0
            for attempt in range(4):
                try:
                    st,v=request('POST','/api/reservations',{'request_id':'race','room_id':1,'date':date,'slot':1},tokens[i])
                except TimeoutError:return 'timeout',busy_attempts
                except Exception:return 'transport_error',busy_attempts
                if st==200:return 'success',busy_attempts
                code=v.get('error',{}).get('code','unknown')
                if code in ('database_busy','overloaded'):
                    busy_attempts+=1
                    if attempt<3:time.sleep(.01*(2**attempt));continue
                return code,busy_attempts
        with concurrent.futures.ThreadPoolExecutor(max_workers=20) as pool:results=list(pool.map(race,range(20)))
        summary={key:0 for key in ['success','capacity_full','conflict','database_busy','overloaded','timeout','transport_error','internal_error']}
        summary.update(collections.Counter(x[0] for x in results));summary['busy_attempts']=sum(x[1] for x in results)
        assert summary['success']==10 and summary['capacity_full']==10,summary
        assert sum(v for k,v in summary.items() if k not in ('success','capacity_full','busy_attempts'))==0,summary
        with sqlite3.connect(database) as db:
            assert db.execute('SELECT count(*) FROM slots s WHERE occupied != (SELECT count(*) FROM reservations r WHERE r.room_id=s.room_id AND r.date=s.date AND r.slot=s.slot AND r.status IN(0,1))').fetchone()[0]==0
            assert db.execute('PRAGMA integrity_check').fetchone()[0]=='ok'
            assert db.execute('PRAGMA foreign_key_check').fetchall()==[]
        stop();start()
        assert expect('GET',f'/api/reservations/detail?id={r["id"]}',token=tokens[0])['status']=='Cancelled'
        assert expect('POST','/api/reservations',data,tokens[0])==r
        rooms=expect('GET',f'/api/rooms?date={date}&slot=1',token=teacher)
        assert rooms[0]['occupied']==10 and rooms[1]['occupied']==0
        expect('POST','/api/logout',{},tokens[0]);expect('GET','/api/me',token=tokens[0],status=401,code='unauthorized')
        stop();log.flush();log.seek(0);logs=log.read()
        assert password not in logs and not any(t in logs for t in tokens)
        assert 'AddressSanitizer' not in logs and 'runtime error:' not in logs,logs
        print(json.dumps({'http_checks':checks,'response_counts':dict(counts),'concurrency':summary,'restart':'passed','database_invariants':'passed'},ensure_ascii=False,indent=2))
    finally:stop();log.close()
