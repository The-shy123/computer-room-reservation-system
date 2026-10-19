#!/usr/bin/env python3
import argparse,datetime,pathlib,secrets,sqlite3,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--build',type=pathlib.Path,required=True);a=p.parse_args();build=a.build.resolve()
with tempfile.TemporaryDirectory(prefix='seat-crash-') as tmp:
    db=pathlib.Path(tmp)/'test.db';password=secrets.token_hex(16)
    day=datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).date()+datetime.timedelta(days=7)
    while day.weekday()>4:day+=datetime.timedelta(days=1)
    commands=f'bootstrap\n{password}\nlogin admin\n{password}\nroom lab 10\ncapacity 1 {day} 1 10\nquit\n'
    r=subprocess.run([str(build/'seat_cli'),'--db',str(db)],input=commands,text=True,capture_output=True,timeout=10)
    assert r.returncode==0 and 'error' not in r.stdout
    with sqlite3.connect(db) as c:assert c.execute('SELECT occupied FROM slots').fetchone()==(0,)
    subprocess.run([str(build/'seat_crash_writer'),str(db)],check=True,timeout=5)
    with sqlite3.connect(db) as c:
        assert c.execute('SELECT occupied FROM slots').fetchone()==(0,)
        assert c.execute('SELECT count(*) FROM reservations').fetchone()==(0,)
        assert c.execute('PRAGMA integrity_check').fetchone()==('ok',)
    print('Process exit before commit: occupancy rollback and database integrity passed')
