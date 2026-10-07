# 歌词服务器（LyricsServer）架构文档

| 项 | 内容 |
|---|---|
| 状态 | 初稿 |
| 版本 | v1.0 |
| 日期 | 2026-10-07 |
| 对象 | 现有 C++ `lyrics-server` + Lua/OpenResty 层（不含架构迁移） |
| 配套 | `requirements.md`、`security-review.md` |

---

## 1. 总体架构

现有系统为**两层、同机部署**，回环直连：

```
浏览器 ──► OpenResty │ Lua Web 层（viewlyrics.com / crintsoft.com / search.crintsoft.com）
                     │   模板渲染、Session、静态资源
                     │   │ resty.http 直连
                     └───▼──────────────────────────────┐
桌面客户端 ──► searchlyrics.htm（ML 协议，经 OpenResty 反代）│
                        │                              ▼
                    ┌───┴───────────────────────────────┐
                    │   C++ lyrics-server  （127.0.0.1:8101）│
                    │   ClientApis / DbApi / DbModify /   │
                    │   LyricsFile / DataSync / Status     │
                    │   SQLite(lyrics.db, users.db)        │
                    │   lyrics/ 歌词文件目录                │
                    └──────────────────────────────────────┘
```

| 层次 | 语言 | 主要职责 |
|---|---|---|
| Web/接入层 | Lua 5.1 + OpenResty | 站点页面、用户会话、静态、把内部 DB 调用转发给后端 |
| 后端 | C++17 + libuv | 客户端协议、数据库读写、歌词文件、同步、状态 |

**端口**：C++ 后端 `127.0.0.1:8101`（master）/ `8100`（slave），Nginx 反向代理到对应 upstream。MySQL 不涉及；存储为 SQLite。

---

## 2. 后端模块与职责（按文件）

| 文件 | 职责 |
|---|---|
| `main.cpp` | 启动、配置读取、信号处理、CLI 模式、工具模式 |
| `LyricsServer.cpp` | 服务编排：初始化 DB/过滤/同步、业务方法（addNewLyrics / updateLyrics / saveLyricsFile / deleteLyricsFile）、状态 dump、定时状态日志、数据同步执行 |
| `LyricsDB.cpp/.h` | lyrics 表增查改、搜索 SQL、别名表（artist/title） |
| `UserDB.cpp/.h` | users 表登录/取 ID |
| `LyricsInfo.cpp` | 歌词元信息解析、encryptLyricsID / decryptLyricsID |
| `SpamLyricsFilter.cpp` | 垃圾歌词过滤（名字/内容两级） |
| `DatabaseModifier.cpp` | 通用 create / update / delete 语句生成与执行 |
| `DataSyncLog.cpp` | 同步日志写入（file-lyrics / db-lyrics / db-users） |
| `SyncRemoteMasterData.cpp` | slave 端从 master 拉取同步并回放 |
| `apis/*` | 各 HTTP handler（见 §4） |
| `AesUtil.cpp` | AES-CBC+CRC 加解密（数据同步用） |
| `ServerConfig.cpp/.hpp` | 全局配置 `g_conf` |

依赖：`../../../Utils`（文件/字符串/JSON）、`../../../TinyJS`（工具）、`HttpLib/HttpServer`（libuv HTTP 服务器）、sqlite3/tiny-AES/glog。

---

## 3. 数据模型

库文件：`<root>/database/lyrics.db`、`<root>/database/users.db`。

### 3.1 lyrics

```
id INTEGER PK AUTOINCREMENT
content_type  INTEGER      -- LRC / TXT
media_length  INTEGER      -- 秒
rate_total    INTEGER DEFAULT 0
rate_count    INTEGER DEFAULT 0
dl_count      INTEGER DEFAULT 0
uploader_id   INTEGER
upload_time   INTEGER
artist / artistcmp / album / title / titlecmp  TEXT
related_link  TEXT
edited_by     TEXT
digest        INTEGER      -- 内容摘要（去重）
索引：lyrics_artistcmp(artistcmp) / lyrics_titlecmp(titlecmp) / lyrics_uploader_id(uploader_id)
```

### 3.2 users

```
id INTEGER PK AUTOINCREMENT
MLPasswordHash  TEXT        -- 客户端(MiniLyrics)哈希
PasswordHash    TEXT        -- Web 哈希
Email           TEXT COLLATE nocase
CreateDate / LastLoginDate / LastPasswordChangedDate
UserName        TEXT COLLATE nocase
唯一索引：users_UserName(UserName) / users_Email(Email)
```

### 3.3 【新增】扩展列（ALTER TABLE，向后兼容追加）

```
-- 歌曲版本（FR-9）
ALTER TABLE lyrics ADD COLUMN version TEXT;
CREATE INDEX lyrics_version ON lyrics (version);
-- 歌词质量分（FR-8）
ALTER TABLE lyrics ADD COLUMN score REAL DEFAULT NULL;
CREATE INDEX lyrics_score   ON lyrics (score);
```

旧记录两列均为 NULL（等价"未标注 / 未打分"），不影响既有 SQL 命中。

---

## 4. 接口设计（现状规格）

### 4.1 客户端协议 `POST /searchlyrics.htm`

- 请求体 = `MLEncodePacketV1`（二进制头 + XOR 载荷），内为 XML 命令，按根节点分派：login / search / batchsearch / upload。
- **包格式**（`MLProtocol.h` / `MLProtocol.cpp`）：
  - 头（22B）：`byVersion`(u8,=2) + `byXor`(u8) + `byID[4]`(LE) + `byMd5[16]`。
  - XOR：载荷逐字节 `^= byXor`（byXor=明文平均字节值）。
  - MD5：`MD5(明文载荷 || 帐户口令)`；帐户口令表 `{4:"Mlv1clt4.0"}`。
- **login**：`LoginUser(name, strPwdMask)`，strPwdMask 即客户端 MLPasswordHash。
- **search**：artist/title 经 `CLyricsKeywordFilter` 归一化 → `SearchLyricsByArtistTitle`，命中即返回；否则按标题/歌手退化为单项搜索（LIMIT 50）。返回项含 `file/saveName` 与 `strServerUrl(http://search.crintsoft.com/l/)`。
- **batchsearch**：每项 `searchBestMatchLyrics`，取集合内最优。
- **upload**：需登录 → 垃圾过滤（名字/内容）→ digest 去重 → 落盘 + 写库 + 写同步日志；返回 `encryptLyricsID(id)`，写回文件 `[id:]` 标签。
- 旧命令（CAN_UPLOAD/UPLOADV0/SEARCH_V0/ACTIVATE）返回既定占位/拒绝。
- 仅 POST；坏包 → `400`。

### 4.2 数据库透传 `POST /db-api/{lyrics,users}`

- `action=prepare` `{sql}` → `{stmt-id=md5(sql), col-names}`（缓存语句）。
- `action=query` `{stmt-id, format=array|dict, args[], col-names?}` → `{rows}`（arg 类型仅空/布尔/整/浮点/字符串）。
- `action=exec` `{sql, format, args}` → 一次 prepare+step。
- 包裹响应：`{result:"OK", message?, …}`；未命中的语句 → `STMT-NOT-EXISTS`（调用方重试）。
- 无认证（内部信任边界，见 security-review H2）。

### 4.3 数据库写 `POST /db-modify-api/{lyrics,users}`

- `{action:create|update|delete, fields:"col,…", args:[…]}` → `{id, result, message?}`。
- create 用 `INSERT ... VALUES (?) RETURNING id`；update 用 `SET {fields}=?`；delete 用 `id=?`。
- 写入后追加数据同步日志。
- 仅 master 注册。

### 4.4 歌词文件 `POST /lyrics-api/`

- `{action:"delete", related-link}` → 删磁盘文件 + 同步日志；仅 master。

### 4.5 主从同步 `POST /api-i/data-sync`

- AES 加密 JSON：`{filename, offset}` → `{filename, offset, data, size, end}`。
- 逻辑：offset==文件尾且当前文件 → `end:true`；否则滚到下一个日志文件从 0 开始；文件不存在 → `FILE-NOT-FOUND`。

### 4.6 状态

- `GET /status/`：启动时间 + 各 handler 计数（search/not-found/batch/login/upload/exists/failed/old/bad）。
- `GET /`：`Service is online.`
- 定时状态日志（`lyr-svr-status.log`，滚动）。

---

## 5. 加密与协议

| 协议 | 机制 | 位置 |
|---|---|---|
| 客户端包 | MD5 + XOR（见 4.1） | MLProtocol |
| 歌词 ID | LE 4B + CRC 首字节 + MLIdEncode | LyricsInfo.cpp |
| 数据同步 | AES-256-CBC `[IV][密文][len:4BE][crc32:4BE]`，CRC 校验明文字节 | AesUtil.cpp |
| Web/客户端密码 | MD5（见 security-review M2） | db_users.lua / UserDB |

---

## 6 搜索与排序

```
归一化(user input) → 别名展开(artist/title alias)
→ 作品命中集合：SearchLyricsByArtistTitle / SearchLyricsByTitle / SearchLyricsByArtist（LIMIT 50）
→ search：直接按 DB 序返回集合；
   batchsearch：bestMatch 取最优（值=基础分 60/70(TXT/LRC) + 评分/时长/专辑 修正）
```

### 6.1 【新增】歌词打分模型（FR-8）

设计为纯函数：`score = Σ wᵢ·fᵢ / Σ wᵢ · 100`，fᵢ∈[0,1]：

| 子指标 fᵢ | 说明 |
|---|---|
| 时间轴完整 | LRC 时间戳覆盖比例、末行是否含结束时间 |
| 内容密度 | 有效行数、字符量、去重后覆盖率 |
| 元数据完整 | artist/title/album 非空 |
| 结构健康 | 无乱码 / 无连续重复 / 无纯数字 / 编码检测 |
| 版本标注 | 识别到版本不惩罚（与基础分解耦） |
| 历史反馈（排序用） | 现 rate_total/rate_count、dl_count 无偏化 |

- score 只反映内容质量，落 `lyrics.score`；排序可用 `final = a·score + b·feedback`。
- `new-score-ranking` 开关，默认开、可回退旧自然序。
- `tool rescore` 批量多线程重算。

---

## 7 歌曲版本（FR-9）

- 规范化小写枚举（开放集合）：`live / remix / acoustic / studio / karaoke / instrumental / cover / other`。
- 解析优先级：显式字段 > 标题解析（中英文词表，`data/` 配置文件化）。
- 落 `lyrics.version`；`/db-api/lyrics` 透传 SQL 天然支持 `SELECT version` / `WHERE version=?`；写接口与搜索结果可选暴露。
- `tool parse-versions` 批量回填。

---

## 8 数据同步

- master：业务写时同步追加 JSONL 日志，按日期滚动；`commit-lyrics.sh` 定时提交日志。
- slave：后台线程循环拉取（`sync-master-url`），逐行回放（文件写删 + `DatabaseModifier` 语义），断点 `offset` 续传、每 10 条落一次进度。
- 新增列（score/version）的变更随 `db-lyrics` 同步入从库，两端一致。

---

## 9 歌词文件存储

- 布局：`<root>/lyrics/<uploadDir>/YYMMDD/<前缀><encId>.lrc|.txt`。
- related_link = 相对 lyricsDir 的路径；下载 URL 为 `http://search.crintsoft.com/l/<related_link>`。
- 文件头/尾写 `[id: <encId>]`（LRC）或 `id: <encId>`（TXT）；编码/换行处理保留。
- 上传写库+写文件+同步日志三件套；删除需同时删库行+文件+日志。

---

## 10 错误码与响应约定

- 包/消息错误 → HTTP `400`；路由未命中 → `404`。
- JSON API 统一包裹 `{result, message?}`；`result` 枚举：`OK / BAD-MESSAGE-FORMAT / BAD-PARAMS / SQL_* / STMT-NOT-EXISTS / INVALID-ACTION / DECRYPT-FAILED / FILE-NOT-FOUND / OFFSET-OUT-OF-RANGE` 等。
- 业务错误（ERP_*）由各 handler 映射为非 200 或 result 非 OK。

---

## 11 安全现状与加固方向

详见 `security-review.md`。本架构落地时必须消除：

- **H1** 写接口 `fields` SQL 注入 → 字段白名单。
- **H2** 内部 API 无认证 → 能力边界/内部鉴权。
- **H3** `related-link` 路径穿越 → canonicalize + 目录校验。
- **M2** MD5 弱哈希 → 新注册迁强 KDF，旧值兼容。
- **M3** data-sync 无认证 → 加 HMAC。

---

## 12 关键新功能落点（现有文件）

| 新功能 | 建议落点 |
|---|---|
| 打分模型 | 新增 `LyricsScorer`（core）+ 入库/排序接入 `LyricsDB.cpp`、`ClientApisHandler.cpp` |
| 打分重算 | `tool LyricTool` 增加 rescore 模式 |
| 版本解析 | 新增 `LyricsVersionParser`（core）+ `LyricsInfo.cpp` + `LyricsDB.cpp` 列读写 |
| 版本回填 | `tool` 增加 parse-versions 模式 |
| schema 迁移 | 启动 `PRAGMA user_version` 幂等迁移，新增列 + 索引 |
| 安全修复 | 见 security-review → 各 handler / DatabaseModifier / LyricsServer 对应处 |