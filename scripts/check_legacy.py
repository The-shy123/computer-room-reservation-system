#!/usr/bin/env python3
"""Read-only structural audit. Never prints account passwords or infers dates."""
import argparse,json,pathlib
p=argparse.ArgumentParser();p.add_argument('directory',type=pathlib.Path);a=p.parse_args()
report={'files':{},'migration':'blocked_for_reservations_without_explicit_actual_date_mapping'}
for name,width in [('admin.txt',2),('student.txt',3),('teacher.txt',3),('computer.txt',2),('order.txt',6)]:
    path=a.directory/name;issues=[];count=0;ids=set()
    if not path.is_file():issues.append({'error':'missing_file'})
    else:
        for number,line in enumerate(path.read_text(encoding='utf-8').splitlines(),1):
            if not line.strip():continue
            count+=1;parts=line.split()
            if len(parts)!=width:issues.append({'line':number,'error':'wrong_field_count'});continue
            if name=='order.txt':
                fields={};bad=False
                for item in parts:
                    if ':' not in item:bad=True;continue
                    key,value=item.split(':',1)
                    if key in fields:bad=True
                    fields[key]=value
                required={'date','interval','studId','stuName','roomId','status'}
                if bad or set(fields)!=required:issues.append({'line':number,'error':'inconsistent_keys'})
                if any(not v for v in fields.values()):issues.append({'line':number,'error':'empty_value'})
                for key,allowed in [('date',range(1,6)),('interval',(1,2)),('status',(-1,0,1,2))]:
                    try:valid=int(fields.get(key,'')) in allowed
                    except ValueError:valid=False
                    if not valid:issues.append({'line':number,'error':'invalid_'+key})
                issues.append({'line':number,'error':'actual_date_missing'})
            elif name!='admin.txt':
                try:
                    identity=int(parts[0]);assert identity>0
                    if identity in ids:issues.append({'line':number,'error':'duplicate_id'})
                    ids.add(identity)
                    if name=='computer.txt':assert int(parts[1])>=0
                except (ValueError,AssertionError):issues.append({'line':number,'error':'invalid_number'})
    report['files'][name]={'records':count,'issues':issues}
print(json.dumps(report,ensure_ascii=False,indent=2))
