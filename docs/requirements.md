# 歌词服务器（LyricsServer）需求文档

---

## 1. 背景与目标

### 1.1 现状

线上歌词服务由**两层**组成：

1. **OpenResty（Nginx + Lua）Web/接入层**（`LyricServer/product/lua/*` + `mlserver.conf`）
   - 对外 HTTP/HTTPS 站点、模板页面、用户会话、静态资源。
   - 用 `resty.http` 把 `db-api / db-modify-api / lyrics-api` 请求转发给 C++ 服务。
2. **C++ `lyrics-server` 后端**（`LyricServer/LyricsServer/*`）
   - 监听 `127.0.0.1:8101`，提供客户端搜索协议、数据库透传、写接口、歌词文件管理、主从同步、状态。

技术栈：C++17 + libuv + sqlite3 + rapidjson + TinyXML + tiny-AES；脚本层 Lua 5.1/OpenResty。

### 1.2 目标

- **梳理并固化** 现有 LyricsServer 的**需求与架构**，形成基线文档。
- **新增两项能力**，并在**现有代码**中实现（不做架构迁移、不换技术栈）：
  1. **歌词打分（quality scoring）**：为每份歌词生成可解释质量分，用于搜索结果排序。
  2. **歌曲版本信息**：区分 live / remix / acoustic 等，支持筛选与搜索。

---

## 2. 范围

### 2.1 本次范围

| 模块 | 说明 |
|---|---|
| 客户端搜索协议 | login / search / batch-search / upload，含包封装与 ID 加密 |
| 数据库透传 API | prepare / query / exec |
| 数据库写 API | create / update / delete（lyrics、users） |
| 歌词文件管理 API | delete-by-link |
| 歌词文件存储/访问 | `/l/`、`/lyrics/` 静态访问，related_link 解析 |
| Web 用户接口 | signin / signout / signup / password_reset / profile / change-password / uploaded-lyrics |
| 会话管理 | Session cookie、登录态 |
| 主从数据同步 | master→slave：文件 + DB 变更 |
| 状态监控 | `/status/`、状态日志 |
| 关键词/垃圾过滤 | 关键字表、垃圾歌词过滤 |
| **新增**歌词打分 | 评分模型、评分存储、排序接入、批量重算 |
| **新增**歌曲版本 | schema 扩展、解析、API 暴露、筛选 |
| CLI / 批量工具 | `--decode-id`、`--parse-tags`、批量压缩/补库 |

## 3. 现状接口盘点（兼容基准）

> 以"现状 = 规格"，逐条对齐。调用方分四类。

### 3.1 客户端（桌面播放器/MiniLyrics）

- **POST `/searchlyrics.htm`**：`MLEncodePacketV1` 封装的 XML 命令（login / search / batchsearch / upload），响应同封装。
- **歌词下载**：`GET /l/<related_link>`。
- 旧命令（CAN_UPLOAD / UPLOADV0 / SEARCH_V0 / ACTIVATE）不再支持。

### 3.2 Web 站点（浏览器）

- 用户：`/user/signin(.aspx)`、`/user/signout`、`/user/signup(.aspx)`、`/user/password_reset`、`/user/profile`、`/user/change-password`、`/user/uploaded-lyrics`、`/api/usr/lyrics/uploaded`、`/api/user/lyrics/delete`。
- 站点页（ViewlyricsMain / crintsoftMain）、静态 `/static/`、`/download/`、`/.well-known/`。

### 3.3 Lua 后端 → C++ 后端（内部被转发）

| URI | 动作 |
|---|---|
| `/db-api/lyrics`、`/db-api/users` | prepare / query / exec（`stmt-id = md5(sql)`） |
| `/db-modify-api/lyrics`、`/db-modify-api/users` | create / update / delete |
| `/lyrics-api/` | delete（删歌词文件） |
| `/status/` | GET 状态 |

### 3.4 主从同步

- `/api-i/data-sync`（AES 加密 JSON）：slave 按 filename+offset 拉取；JSONL 日志 `{type:file-lyrics|db-…, action:create|update|delete,…}`。

### 3.5 数据与文件

- SQLite `lyrics.db`、`users.db`（schema 见 §3.6）。
- 歌词文件：`<root>/lyrics/<uploadDir>/YYMMDD/xx_<encId>.lrc|.txt`，含 `[id: <encId>]` / `id: <encId>` 标签。
- 过滤表 `data/LyrKeywordFilter.xml`；歌手/标题别名表。

### 3.6 数据库 Schema

**lyrics**：`id, content_type, media_length, rate_total, rate_count, dl_count, uploader_id, upload_time, artist, artistcmp, album, title, titlecmp, related_link, edited_by, digest`
索引：`lyrics_artistcmp(artistcmp)`、`lyrics_titlecmp(titlecmp)`、`lyrics_uploader_id(uploader_id)`

**users**：`id, MLPasswordHash, PasswordHash, Email(collate nocase), CreateDate, LastLoginDate, LastPasswordChangedDate, UserName(collate nocase)`
唯一：`users_UserName`、`users_Email`

> 新增字段见 §6 / §7，用 `ALTER TABLE ADD COLUMN` 向后兼容。

---

## 4 功能需求（FR）

### FR-1 客户端搜索协议兼容
完整实现 login / search / batch-search / upload；包封装 `MPV_V1_MD5_ID`、ID 加密、关键词过滤、best-match 排序与现状一致。旧客户端用例回归、坏包按现状错误码返回。

### FR-2 搜索排序"作品优先"
artist+title 都命中时优先返回该集合；否则退化为按标题/按歌手。集合内部排序现为自然序（digest 去重），**引入打分后优化为按分数降序**（开关可回退）。

### FR-3 数据库透传 API 兼容
`/db-api/*` 支持 prepare/query/exec，保留 `stmt-id=md5(sql)`、`format=array|dict`、`col-names`、`result` 枚举与响应包裹；prepare 缓存语义一致；`STMT-NOT-EXISTS` 触发脚本重试。

### FR-4 数据库写 API 兼容
`/db-modify-api/*` 支持 create/update/delete，返回 `{id, result, message?}`，写同步日志。同时**修正**字段注入面（见 security-review H1）。

### FR-5 用户 Web 接口兼容
迁移/保留 Session、所有 `/user/*` 页面与两个 `/api/...` JSON 路由，URL、表单字段、返回、重定向一致。

### FR-6 歌词文件存储与访问
文件命名/路径/`[id:]` 标签、related_link 生成、`/l/`+`/lyrics/` 静态访问一致；删除时同步删文件+写同步日志；**修复路径穿越**（security-review H3）。

### FR-7 主从数据同步
slave 拉取并回放；master 按日期滚动日志；断点按 offset 续传；`test-all.sh` 双库双目录比对一致。

### FR-8 【新增】歌词打分
- 为每份歌词生成 **0–100 质量分**，用于：搜索排序首要因子、上传建议分、低质垃圾识别、管理展示。
- 子指标（权重可配）：时间轴完整性、内容密度、元数据完整、结构健康、版本信号；历史反馈（rate/dl）作为排序补充信号。
- 存储：`lyrics.score REAL DEFAULT NULL`；**可重算**（提供批量重算工具），不破坏原始字段。
- 验收：好坏样例区分合理；5 万行批量重算 1 小时内完成；排序生效且可配置开关。

### FR-9 【新增】歌曲版本
- 为歌词标注版本：live / remix / acoustic / studio / karaoke / instrumental / cover 等。
- 来源：显式字段 > 标题解析（词表可配）。
- 存储：`lyrics.version TEXT` + 索引；`/db-api` 可 SELECT / WHERE；搜索与管理接口可见。
- 验收：中英文解析样例准确；能按 `version='live'` 过滤出正确子集。

### FR-10 状态与可观测性
`/status/` 结构兼容；聚合计数（search/batch/login/upload 等）；结构化日志、崩溃指标。

### FR-11 CLI 与批量工具
`--decode-id`、`--parse-tags` 行为一致；批量处理目录（压缩内容、更新 digest、补漏入库）；新增 `rescore`、`parse-versions` 子命令。

---

## 5 非功能需求（NFR）

| 编号 | 分类 | 要求 |
|---|---|---|
| NFR-1 | 性能 | 单进程吞吐不劣化；搜索 P99 保持；大目录（40GB、大 inode）可访问 |
| NFR-2 | 并发 | 写路径并发安全，避免 SQLite 写锁竞争 |
| NFR-3 | 安全 | 包 MD5 校验、密码哈希、data-sync AES-CBC+CRC 与现状等价；**消除 security-review 列表的高/中危项** |
| NFR-4 | 兼容 | 客户端与 Web 调用方零改动 |
| NFR-5 | 可靠性 | 进程崩溃可快速重启（systemd）；同步断点续传；日志按大小滚动 |
| NFR-6 | 可维护 | 模块边界清晰、功能开关化（打分开关、版本解析开关）、文档齐全 |
| NFR-7 | 可移植 | macOS 开发/调试，Linux 部署（沿用现状 Ubuntu 环境） |

---

## 6 验收标准

**阶段 A — 兼容回归**：旧客户端搜索/上传通过；Lua 调用面等价。
**阶段 B — 数据与同步**：master/slave 建库、上传、同步、双端比对全过（沿用 `test-all.sh`）。
**阶段 C — 新功能**：打分排序生效、可批量重算；版本解析/入库/查询/过滤可见。
**阶段 D — 安全**：security-review 中 H1/H2/H3、M2/M3 等已修复项回归通过（含注入、越权、路径穿越用例）。

---

## 7 附录：名词

- **cmp 字段**：经 `LyricsKeywordFilter` 归一化后的 artist/title，用于检索。
- **related_link**：歌词文件相对 lyricsDir 的路径，也是下载 URL 后缀。
- **digest**：歌词内容摘要，用于去重。
- **MLProtocol**：桌面端 XML-over-加密包 通信协议。