#!/usr/bin/env python3
import argparse,datetime,pathlib,secrets,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--cli',type=pathlib.Path,required=True);a=p.parse_args()
with tempfile.TemporaryDirectory(prefix='seat-cli-') as tmp:
    password=secrets.token_hex(16);db=str(pathlib.Path(tmp)/'test.db')
    day=datetime.datetime.now(datetime.timezone(datetime.timedelta(hours=8))).date()+datetime.timedelta(days=7)
    while day.weekday()>4:day+=datetime.timedelta(days=1)
    data=f'bootstrap\n{password}\nlogin admin\n{password}\nuser s 0\n{password}\nroom lab 1\nlogin s\n{password}\napply first 1 {day} 1\nlist\nchange 1 Pending Cancelled\nroom lab nope\nunknown\n'
    r=subprocess.run([str(a.cli.resolve()),'--db',db],input=data,text=True,capture_output=True,timeout=10)
    assert r.returncode==0 and 'Pending' in r.stdout and 'Cancelled' in r.stdout and r.stdout.count('invalid_input:')==2,(r.stdout,r.stderr)
    assert password not in r.stdout
    r=subprocess.run([str(a.cli.resolve()),'--db',db],input=f'login s\n{password}\nlist\nquit\n',text=True,capture_output=True,timeout=5)
    assert r.returncode==0 and 'Cancelled' in r.stdout
    # A real pseudo-terminal verifies the interactive echo flag, not just piped output.
    import os,pty,select,termios,time
    master,slave=pty.openpty()
    terminal=subprocess.Popen([str(a.cli.resolve()),'--db',str(pathlib.Path(tmp)/'terminal.db')],stdin=slave,stdout=slave,stderr=slave)
    transcript=bytearray()
    def until(marker):
        chunk=bytearray();deadline=time.monotonic()+5
        while marker not in chunk:
            if time.monotonic()>deadline:raise AssertionError('terminal timeout')
            if select.select([master],[],[],.1)[0]:
                part=os.read(master,4096)
                if not part:raise AssertionError('terminal closed')
                chunk.extend(part);transcript.extend(part)
        return bytes(chunk)
    try:
        until(b'> ');os.write(master,b'bootstrap\n');until(b'Password: ')
        assert not (termios.tcgetattr(slave)[3]&termios.ECHO)
        os.write(master,password.encode()+b'\n');until(b'> ')
        assert termios.tcgetattr(slave)[3]&termios.ECHO
        assert password.encode() not in transcript
        os.write(master,b'quit\n');assert terminal.wait(timeout=5)==0
    finally:
        if terminal.poll() is None:terminal.kill();terminal.wait()
        os.close(master);os.close(slave)
    print('CLI workflow, real terminal echo suppression/restoration, invalid input, EOF and reopen: passed')
