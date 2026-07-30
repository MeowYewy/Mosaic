# Mosaic 兑换码网关

这个服务同时完成两件事：

1. 用户输入兑换码后，向 Mosaic 返回模型、调用模式和一个网关令牌。
2. Mosaic 用网关令牌调用 AI；真实的 Kimi / DashScope API Key 始终只保存在服务器 SQLite 中，不下发到客户端。

`remaining_uses` 表示还可首次激活多少个客户端。相同客户端因超时重试或重新输入同一码，不会重复扣次数。已激活客户端不受该数字影响；如需停用，可禁用整个兑换码或撤销单个 activation。

## 本地启动

```bash
cd server/redemption_gateway
python3 admin.py init
python3 gateway.py --host 127.0.0.1 --port 8787
```

健康检查：

```bash
curl http://127.0.0.1:8787/healthz
```

## 浏览器管理后台（推荐）

Windows Server 上通过远程桌面进入服务器，打开 `C:\server\redemption_gateway`（或你上传后的目录），双击：

`打开管理后台.cmd`

### 首次使用：设置登录密码

管理后台现已启用登录保护。首次部署后，在服务器上运行：

```powershell
cd C:\server\redemption_gateway
python admin.py set-admin-password
```

按提示设置用户名（默认 `admin`）和密码（至少 12 位）。凭证保存在
`data\admin_credentials.json`，不会写入 git。

### 本地访问

脚本会自动打开浏览器中的 `http://127.0.0.1:8788/login`。登录后可以：

- 创建兑换码并复制；
- 修改备注、模型、API 地址、剩余次数和到期时间；
- 更换真实 API Key；
- 启用或停用整个兑换码；
- 查看已激活设备，撤销或恢复某一台设备。

管理页默认只允许监听服务器本机（`127.0.0.1:8788`）。关闭启动它的窗口就会停止。

### 通过域名访问（可选）

若希望通过 `https://page-case.com/gateway-admin/` 访问管理后台：

1. 宝塔 → 站点 → 反向代理，新增：
   - 代理目录：`/gateway-admin/`
   - 目标 URL：`http://127.0.0.1:8788/`
   - 缓存：关闭

2. 启动管理后台时带上路径前缀：

```powershell
python admin_web.py --base-path /gateway-admin --open-browser
```

或在 `windows\open-admin-panel.ps1` 中增加 `--base-path /gateway-admin`。

3. **不要**在腾讯云防火墙中开放 8788；仅通过 HTTPS 反代访问。

登录会话有效期 12 小时，Cookie 为 HttpOnly；连续 15 分钟内失败 10 次会临时锁定该 IP 的登录尝试。

如果不方便双击，也只需记住：

```powershell
python admin_web.py --open-browser
```

## 命令行管理（备用）

以下命令保留用于管理页面无法启动时排查，不需要日常记忆。

### 创建兑换码

千问 OCR：

```bash
python3 admin.py create \
  --label "10台设备-千问OCR" \
  --mode qwen_ocr \
  --api-base https://dashscope.aliyuncs.com/api/v1 \
  --model qwen3.5-ocr \
  --ocr-cloud-mode single \
  --uses 10
```

Kimi 文本：

```bash
python3 admin.py create \
  --label "5台设备-Kimi" \
  --mode text \
  --api-base https://api.moonshot.cn/v1 \
  --model moonshot-v1-8k \
  --uses 5
```

命令会在终端安全提示中读取两次真实 API Key，不把 Key 写进 shell 历史。自动生成的兑换码只显示一次；数据库只保存兑换码哈希。

### 日常管理

```bash
python3 admin.py list

# 修改剩余首次激活次数
python3 admin.py update --id 1 --remaining-uses 20

# 更换真实模型 Key；已激活客户端立即使用新 Key
python3 admin.py update --id 1 --change-api-key

# 修改模型、停用或恢复兑换码
python3 admin.py update --id 1 --model qwen3.5-ocr
python3 admin.py update --id 1 --enabled no
python3 admin.py update --id 1 --enabled yes

# 查看并撤销某个客户端
python3 admin.py activations --id 1
python3 admin.py revoke-activation --activation-id 3
```

## Windows Server 2022 + 宝塔部署

### 推荐：宝塔反向代理

1. 将本目录上传到服务器网站目录之外，例如：

   `C:\MosaicGateway`

   不建议放在宝塔网站根目录，以免配置失误时暴露 SQLite 数据库和真实
   API Key。

2. 先在当前测试窗口按 `Ctrl+C` 停止手动启动的网关，再以管理员身份打开
   PowerShell，安装开机自启任务。假设宝塔站点使用
   `http://111.229.133.160`：

```powershell
cd C:\MosaicGateway
powershell -ExecutionPolicy Bypass -File .\windows\install-windows-task.ps1 `
  -PublicBaseUrl "http://111.229.133.160"
```

脚本会让网关监听 `127.0.0.1:8787`、初始化数据库，并创建名为
`Mosaic Redemption Gateway` 的 Windows 计划任务。运行日志在
`data\gateway.log`。

3. 宝塔面板中创建一个专用站点，进入该站点的“反向代理”，添加：

   - 代理目录：`/`
   - 目标 URL：`http://127.0.0.1:8787`
   - 缓存：关闭
   - 发送域名：使用站点域名

4. 验证：

```powershell
Invoke-RestMethod http://111.229.133.160/healthz
```

返回 `ok : True` 即成功。浏览器直接打开站点根地址会显示“Mosaic
兑换码网关，服务运行正常”。

### 没有域名：直接开放 8787

```powershell
cd C:\MosaicGateway
powershell -ExecutionPolicy Bypass -File .\windows\install-windows-task.ps1 `
  -PublicBaseUrl "http://你的服务器公网IP:8787" `
  -ListenHost "0.0.0.0" `
  -OpenFirewall
```

这种方式还需要在腾讯云轻量服务器防火墙中放行 TCP `8787`。若使用宝塔
反向代理，则不需要对公网开放 8787。

查看任务状态或重启：

```powershell
Get-ScheduledTask -TaskName "Mosaic Redemption Gateway"
Get-ScheduledTaskInfo -TaskName "Mosaic Redemption Gateway"
Stop-ScheduledTask -TaskName "Mosaic Redemption Gateway"
Start-ScheduledTask -TaskName "Mosaic Redemption Gateway"
```

移除开机任务但保留数据库：

```powershell
powershell -ExecutionPolicy Bypass -File .\windows\remove-windows-task.ps1
```

默认最多同时处理 8 个请求，适合 2 核 2 GB 的轻量服务器。

## Linux 备用部署

若以后迁移到 Linux，可使用 `systemd/mosaic-redemption.service` 和
`mosaic-redemption.env.example`。建议把程序放到
`/opt/mosaic-redemption`，数据库放到 `/var/lib/mosaic-redemption`。

## 客户端地址

发布 Mosaic 前，把 `resources/redemption.json` 中的 `baseUrl` 改为：

```json
{"baseUrl":"http://111.229.133.160"}
```

如果不使用宝塔反向代理、而是直接开放端口，则改为
`http://服务器公网IP:8787`。

开发调试时也可临时设置环境变量 `MOSAIC_REDEMPTION_SERVER_URL` 覆盖资源文件。

## 备份

Windows 服务器上先停止计划任务，再备份：

```powershell
Stop-ScheduledTask -TaskName "Mosaic Redemption Gateway"
Copy-Item .\data\gateway.sqlite3 D:\Backup\gateway.sqlite3
Start-ScheduledTask -TaskName "Mosaic Redemption Gateway"
```

服务器数据库包含真实模型 Key，应限制 `data` 目录权限，不要通过宝塔网站
公开该目录。
