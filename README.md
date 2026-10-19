# 机房座位预约服务

把原 C++ 命令行课程项目原位升级为 **C++17 + SQLite + Drogon** 服务。保留学生申请、教师审核、学生取消的业务流程；使用实际日期、统一状态、事务与数据库约束保证预约一致性。提供 CLI 和 JSON HTTP API，不依赖前端。

当前可演示：

- 登录与 8 小时会话；学生仅查看/取消自己的预约，教师查询与审核，管理员创建账号/机房、调整指定时段容量。
- 工作日、上午/下午的机房容量预约；Pending、Approved 占位，拒绝/取消释放。
- 稳定预约 ID、跨机房重复时段约束、按用户隔离的幂等申请。
- 条件扣减、预期状态更新、原子事务、操作记录、数据库重开与进程重启恢复。
- 密码使用 OpenSSL PBKDF2-HMAC-SHA256，随机盐；不展示密码。会话只保存令牌摘要。

原始五个 `.txt` 文件在已有本地目录中保留，**新程序不会读取或覆盖它们**。旧文本数据已全部停止 Git 跟踪并加入忽略规则；新克隆通过演示脚本生成独立数据，不包含旧账号、预约或机房数据。旧程序源文件已由当前分层代码替换。没有清空历史的业务接口。不要使用 `g++ *.cpp`：请按 CMake 构建。

## 构建

本次验证环境：macOS 27.0.1 arm64、AppleClang 21、CMake 4.4.4、Python 3.13.3。依赖锁定：系统 SQLite 3.54.0、已有 OpenSSL 3.6.3、内置 doctest 2.4.12；HTTP 按提交拉取 Drogon 1.9.9、Trantor 和 JsonCpp 1.9.6。详情和哈希见 `docs/design.md`。不安装全局组件。

在项目根目录执行：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

HTTP 首次配置需要下载固定源码，之后保存在构建目录：

```sh
cmake -S . -B build-http -DCMAKE_BUILD_TYPE=Debug -DSEAT_HTTP=ON
cmake --build build-http -j4
ctest --test-dir build-http --output-on-failure
python3 scripts/http_smoke.py --build build-http
```

最后一条命令是完整演示：生成独立临时数据库和随机测试密码，启动 **127.0.0.1** 服务，执行流程/错误场景/20 客户端竞争/重启，打印分类结果后关闭服务并清理临时数据。只对这个本地测试禁用代理。它是正确性演示，不是性能基准。

若环境缺少精确版本，CMake 会明确失败。可通过 `OPENSSL_ROOT_DIR`、`CMAKE_PREFIX_PATH` 指向项目级安装目录；不要为了通过配置而直接取消版本检查。其他平台尚未验证；CLI 的隐藏密码输入目前使用 POSIX termios。

## 交互演示

```sh
mkdir -p work
./build-http/seat_cli --db "$PWD/work/demo.db"
```

CLI 首次依次输入下列命令；`bootstrap`、`login`、`user` 会另行提示密码，终端关闭密码回显。请选择自己的演示密码，不在 shell 参数中写密码。

```text
bootstrap
login admin
user student1 0
user teacher1 1
room lab1 10
users
login student1
rooms <未来工作日YYYY-MM-DD> 1
apply demo-001 1 <同一日期> 1
list
login teacher1
list
change 1 Pending Approved
login student1
change 1 Approved Cancelled
quit
```

将尖括号内容替换为未来工作日。`1` 为上午 08:00 开始，`2` 为下午 14:00 开始；开始时刻不再接受申请。业务时区为 Asia/Shanghai（UTC+08:00）。角色 `0/1/2` 分别为学生/教师/管理员。首次机房/预约 ID 为 1；已有数据时使用实际输出的 ID。同一用户重试相同请求编号与内容会返回同一申请结果；换内容会冲突。取消后再次申请要用新编号。

启动已经初始化的数据库对应的 HTTP 服务：

```sh
./build-http/seat_http --db "$PWD/work/demo.db" --host 127.0.0.1 --port 8080
```

默认仅本机演示，不含 TLS；当前没有上线或公网部署验证。退出按 Ctrl+C。数据库路径必须是绝对路径、父目录必须存在，不会隐式依赖工作目录查找数据。

## API

成功响应为 JSON；统一错误格式：`{"error":{"code":"conflict","message":"..."}}`。
除登录/health 外，请求头携带 `Authorization: Bearer <token>`。服务端从会话确定身份，预约 JSON 不允许 `student_id` 或 `role`。

|方法与路径|请求内容/权限|
|---|---|
|POST `/api/login`|`name,password` → `token,expires_in`|
|POST `/api/logout`|撤销当前令牌|
|GET `/api/me`|当前身份|
|GET `/api/rooms?date=YYYY-MM-DD&slot=1`|已登录角色查看余量|
|POST `/api/reservations`|学生：`request_id,room_id,date,slot`|
|GET `/api/reservations`|学生本人的记录；教师全部记录|
|GET `/api/reservations/detail?id=1`|学生自己的详情；教师详情|
|POST `/api/reservations/cancel`|学生：`id,expected`|
|POST `/api/reservations/review`|教师：`id,expected,target`；target 仅 Approved/Rejected|
|GET / POST `/api/users`|管理员列表 / 创建 `name,password,role`|
|POST `/api/rooms`|管理员创建 `name,capacity`|
|POST `/api/rooms/capacity`|管理员调整 `room_id,date,slot,capacity`|
|GET `/health`|进程存活探针，不检查数据库健康|

错误：400 输入错误，401 未登录/会话失效，403 权限不足，404 不存在，409 名额不足或冲突（看 code 区分），503 数据库忙碌或队列满，500 存储/内部错误。成功申请和重试均返回 200。

## 验证

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=Debug -DSEAT_HTTP=ON -DSEAT_SANITIZERS=ON
cmake --build build-sanitize -j4
ctest --test-dir build-sanitize --output-on-failure
python3 scripts/check_legacy.py .
```

测试数据库均独立且自动清理。旧数据检查只报告问题，不打印密码、不迁移数据、不推断历史日期。

最近验证记录（2026-10-19，macOS arm64）：

|验证项|实际结果|
|---|---|
|Debug 完整 CTest|4/4 通过：核心、CLI、HTTP、崩溃恢复|
|Release 核心 CTest|3/3 通过；未构建 Release HTTP|
|ASan + UBSan 完整 CTest|4/4 通过，无检测报告；系统 SQLite/OpenSSL 未重新插桩|
|业务核心|14 个用例、177 条断言通过|
|HTTP 流程与错误处理|77 项检查通过，覆盖预期的 400/401/403/404/409/503 响应|
|独立连接与 HTTP 并发|各 20 个参与者竞争 10 个名额，均为 10 成功、10 满额拒绝；最终忙碌、超时、内部错误均为 0|
|回滚与恢复|故障注入、提交失败、进程重启及未提交直接退出均通过，库存与有效预约一致|

这是已运行场景的正确性证据，不是性能基准；未测量吞吐或延迟分位数，Linux/Windows、TSan、真实断电和长期运行尚未验证。详细规则、事务边界、并发策略及依赖版本见 [技术设计](docs/design.md)。

## 许可证

本项目采用 [MIT License](LICENSE)，版权所有 © 2026 Zitao Zhang。第三方组件保留各自的版权与许可证；内置 doctest 的 MIT 版权声明保留在原头文件中，Drogon、Trantor、JsonCpp、SQLite 和 OpenSSL 的许可证以各自上游发行内容为准。
