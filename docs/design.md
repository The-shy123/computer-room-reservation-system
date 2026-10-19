# 业务与工程设计

## 规则与边界

预约一张机房名额，不选具体座位。日期采用严格 YYYY-MM-DD 格式，当前明确支持 2000—2099；按公历验证闰年和月底，禁止周末。时区 Asia/Shanghai 在该范围使用固定 UTC+08:00，不修改系统 TZ。上午 08:00、下午 14:00 开始；等于开始时刻也不能新增。`Clock` 注入 `LocalTime`，业务测试固定到 2026-10-19 07:00，不依赖当天。

状态用 `enum class Status`，数据库编码 Pending=0、Approved=1、Rejected=2、Cancelled=3。与旧 txt 数字含义不同，不可直接复制。

|当前状态|允许目标|占用变化|
|---|---|---|
|Pending|Approved|0|
|Pending|Rejected / Cancelled|-1|
|Approved|Cancelled|-1|
|Rejected / Cancelled|无|无|

只有学生能申请和取消自己的记录；教师查询全部并审核 Pending；管理员管理账号、机房和时段容量，不能代替学生申请/取消或代替教师审核，也不访问预约列表/详情。已登录角色可查询未来时段余量。权限在 Service 检查，HTTP/CLI 不形成安全边界。

没有历史清空 API，不自动过期、不候补、不循环预约、不重新打开终态。过期日期的 Pending/Approved 保留历史占用，不影响其他日期的容量。当前没有停用账号、密码重置、分页、机房删除等扩展管理功能。

## 分层与所有权

- `include/seat/domain.hpp` / `src/domain.cpp`：值类型、强枚举、日期和状态规则，不依赖终端、HTTP、数据库。
- `include/seat/service.hpp` / `src/service.cpp`：应用用例、身份校验、事务边界。对外只暴露业务类型，不返回 SQLite 句柄。固定使用一种 SQLite 实现，SQL 放在内部实现中，不为未来数据库添加抽象接口。
- `include/seat/database.hpp` / `src/database.cpp`：连接、预编译语句、事务 RAII 与版本迁移；所有外部值参数绑定。
- `src/password.cpp`：调用 OpenSSL 的随机数、PBKDF2、摘要和常量时间比较，不自创密码算法。
- `main.cpp`：读取整行、校验参数数量、显示结果；输入错误不留下失败流循环；EOF 退出。终端回显通过局部 RAII 恢复。
- `src/http.cpp`：解析 JSON、认证头、HTTP 错误映射；调用相同 Service，不复制预约规则。

`Service` 用 `unique_ptr<Impl>` 独占连接并隐藏 SQLite 细节；不可复制。`User/Room/Reservation` 用值语义。Connection 必须活得比 Statement/Transaction 久；连接、语句、事务都不可复制，析构不抛异常。

事务构造执行 `BEGIN IMMEDIATE`；只有 COMMIT 成功才标记 committed。异常沿栈返回时析构执行 ROLLBACK。提交失败时也不标记成功。若进程直接退出，析构不会执行，恢复依靠 SQLite 日志机制；`crash_recovery` 专门验证这个区别。

## 数据库 schema v1

初始化用 `PRAGMA user_version`，版本 0 在事务内建立 v1；v1 可重复打开；未知版本明确报错，不擅自降级或重建。并发初始化取得写锁后重新读版本。每个连接启用并查询验证 `foreign_keys=ON`，配置 250ms busy timeout。新文件启用 WAL；SQLite 默认 FULL 同步策略保留。

|表/索引|用途和保护|
|---|---|
|users|稳定主键、唯一账号名、角色 CHECK、密码哈希|
|rooms|稳定主键、唯一名称、默认容量 CHECK 0..1000000|
|slots|主键 `(room_id,date,slot)`，机房外键；容量及 `0 <= occupied <= capacity` CHECK|
|reservations|自增稳定 ID；学生外键；机房/日期/时段组合外键；状态 CHECK|
|one_active_per_student|部分唯一索引 `(student_id,date,slot) WHERE status IN(0,1)`，跨机房防止重复有效占位|
|reservations_slot|按机房时段与状态查询的索引|
|requests|主键 `(student_id,request_id)`；记录原始 room/date/slot 和唯一 reservation_id 外键|
|audit|操作者、关联预约（管理动作可为空）、动作、UTC 时间；与业务变更同事务|
|sessions|256 bit 随机令牌的 SHA-256 摘要、用户外键、到期时间；expiry 索引|

机房默认容量在首次产生具体时段行时复制；管理员调整的是**指定日期时段**容量，不隐式批量修改历史/未来时段。没有单独库存表，occupied 和 capacity 在同一 slots 行。首次申请失败时新建的时段行也一起回滚。

数据库约束不能独自表达所有规则：精确日期/周末/角色和合法状态转移由 Service 校验；库存等于有效预约数依赖以下完整事务协议。直接手改数据库不属于受支持业务路径；回归测试独立计算等式核验，并检查 integrity_check/foreign_key_check。

## 申请事务与幂等

1. BEGIN IMMEDIATE，读取令牌对应学生身份。
2. 查 `(student_id, request_id)`；同内容返回原申请快照（原始 Pending 状态和相同 ID），不同内容返回 Conflict。已有成功请求可以在时段开始后重放；不会重新占位。
3. 验证日期/时段；查询业务重复预约（已持有 SQLite 写事务，最终还有唯一索引兜底）。
4. 建立时段行，执行 `UPDATE slots SET occupied=occupied+1 ... AND occupied<capacity`。影响行数不是 1 才返回 Full。
5. 插入预约、请求记录、审计记录；COMMIT。

名额判定来自条件 UPDATE，不能改成普通 SELECT 后脱离事务再 INSERT。请求记录只保存成功申请；失败事务不占用请求编号，客户端修复问题后可重试。幂等返回申请时的结果；`get` 返回当前状态。取消后以旧编号重试仍得到原申请 ID，不创建新预约。重新申请用新编号。

同内容但不同编号是**业务重复**，通过学生/日期/时段约束拒绝；不同学生可以使用同一编号。这与网络重试幂等是两个独立保证。

## 状态变更与竞争

审核/取消 BEGIN IMMEDIATE 后查身份和记录归属；校验 expected→target；`UPDATE reservations ... WHERE id=? AND status=?` 必须影响一行。需要释放时再条件 `occupied>0` 减一，并插入审计，统一提交。重复请求或过期 expected 返回 Conflict，不重复释放。

审核 Approved 与学生 Cancelled 都提交 expected=Pending 时：

- 审核先赢：最终 Approved，占用 1，取消返回 Conflict。
- 取消先赢：最终 Cancelled，占用 0，审核返回 Conflict。
- 学生重新读取 Approved 后，明确以 expected=Approved 再取消，可到 Cancelled。

两个调用都携带 Pending 时不会同时成功。不要自动把失败的 expected 改成最新状态重试，这会改变调用者意图。重复取消、拒绝后的取消也不会释放第二次。

## SQLite 并发与错误

一个 Service/Connection 只由一个执行线程使用，不共享 Statement。测试中连接可以在创建后整体移交线程，但不交叉并发访问同一个连接。HTTP 每项任务在工作线程创建并销毁自己的 Service/Connection。多进程/多连接通过数据库写锁和约束协作，不依赖进程内互斥锁保障业务一致性。

WAL 允许读写更好地交错，**仍只有一个写事务**。选 BEGIN IMMEDIATE 避免先读后升级写锁的不确定性；PBKDF2 在取得写事务前计算，缩短锁持有时间。数据库内部 busy timeout 为 250ms；到期返回 Busy/HTTP 503。Service 不进行隐藏的整事务循环重试。

测试客户端只对 busy/overloaded 最多尝试 4 次，间隔 10/20/40ms，每次复用原请求编号。实际客户端也必须使用有限预算，并可参考 HTTP Retry-After；Full、Conflict、InvalidInput 不自动重试。250ms 是锁等待上限配置，不是请求总时延承诺。

错误类型：InvalidInput、NotFound、Unauthorized、Forbidden、Full、Conflict、Busy、Storage。SQLite CONSTRAINT 映射 Conflict，BUSY/LOCKED 映射 Busy，其他存储错误归 Storage。测试触发器故意 RAISE(ABORT) 属于约束错误，回滚测试据此断言 Conflict；没有把这种故障称作真实磁盘损坏。

参考：[SQLite 事务](https://www.sqlite.org/lang_transaction.html)、[WAL](https://www.sqlite.org/wal.html)。

## HTTP 与认证生命周期

使用 Trantor 自带 `ConcurrentTaskQueue`，4 个工作线程；Drogon 2 个 IO 线程。所有 Service 操作（包括登录哈希）都进入工作队列，health 是无需 DB 的轻量路由。运行+排队最多 128 个，超出返回 overloaded/503。不自建线程池。

队列捕获 `shared_ptr<Runtime>`、HttpRequestPtr、响应回调与操作函数，均按值拥有；任务内的 Service 是栈对象，不跨回调引用。完成对象递减在途计数；停止时等待已接收任务结束再停止框架队列。客户端断开不等于取消数据库事务；可能已经提交，因此客户端应复用请求编号查询/重试。断连场景已进入 Sanitizer HTTP 测试。

密码策略：OpenSSL PBKDF2-HMAC-SHA256，600000 次，16 字节随机盐、32 字节派生值，保存算法/迭代数/盐/哈希；比较调用 CRYPTO_memcmp。密码长 8—128 字节。随机令牌 32 字节，以十六进制返回；只在数据库保存 SHA-256 摘要；到期 8 小时，退出撤销，重启仍可使用未到期会话。过期会话在下次登录时清理，没有后台预约释放任务。参考：[OpenSSL PBKDF2](https://docs.openssl.org/3.6/man3/PKCS5_PBKDF2_HMAC/)。

此版定位本地学习演示：没有 TLS、登录限速/防爆破、密码重置、严格请求总时限、分页和生产监控；队列限制不等于全套限流。异常信息不含 SQL 参数或凭据；没有开启请求体日志。旧 txt 中的明文凭据保留在本机但不再使用；全部旧文本数据已停止 Git 跟踪。公开仓库使用独立演示数据，不附带旧文本数据。

## 依赖锁定与兼容补丁

|组件|固定版本/来源|
|---|---|
|SQLite|3.54.0，本机 Xcode SDK/系统库；CMake EXACT，测试检查运行库版本与头文件一致|
|OpenSSL|3.6.3，现有 `/opt/homebrew/opt/openssl@3`；CMake EXACT，可配置 prefix|
|doctest|2.4.12，vendored `third_party/doctest.h`，MIT 许可证内含；提交 `1da23a3e8119ec5cce4f9388e91b065e20bf06f5`|
|Drogon|1.9.9，`38dd5fea31a7a2727c0a6f6b6b04252374796cab`|
|Trantor|Drogon submodule `8bf280ba043eb77c3eb02121065e0891a9a4f431`|
|JsonCpp|1.9.6，`89e2973c754a9c02a49974d839779b151e95afd6`|
|Zlib|本次 SDK 检出 1.2.12，HTTP 传递依赖；CMake 锁定该版本|
|线程/UUID|操作系统 SDK，不宣称跨 SDK 二进制可复现|

Drogon/Trantor/JsonCpp 来源分别为 GitHub 官方 drogonframework/drogon、an-tao/trantor、open-source-parsers/jsoncpp。doctest 单头 SHA-256：`94029a7d32da24a56249658147dbd2b33ff0b9ed665295cbbaf19aafff5b0ced`。

`cmake/HttpDependencies.cmake` 可重复地将固定 Trantor 头文件中的 `fut.get();` 改为 `(void)fut.get();`，修复 AppleClang 21/libc++ nodiscard 构建错误；仍调用 get、等待和传播异常，没有关闭错误检查。依赖关闭 ORM/Redis/MySQL/PostgreSQL/示例等非必需功能。第三方 Drogon 有两条 deprecated-declarations、一条头文件 unused-parameter 编译警告，以及旧 CMake 最低版本提示，本项目业务代码编译未产生警告。

旧数据只有星期、字段名称也存在旧程序写入缺陷。`scripts/check_legacy.py` 按行报告结构、字段、数值和缺失日期，不输出密码。旧文件仅保留在已有本地目录，公开仓库不附带旧数据。未开发导入器，也不会把历史星期编号猜成当前周；迁移需要先明确日期映射和错误处理政策。
