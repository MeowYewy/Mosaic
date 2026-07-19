# Mosaic (ProjectO)

本地文档工具：PDF 拆分/合并/旋转/转换/压缩/水印，以及 DOCX / PDF / 图片脱敏（OCR + 手动标注 + 不可逆导出）。

<p align="center">

  <strong>本地 · 离线 · 人工复核</strong><br/>

  PDF 工具箱 · 文档脱敏

</p>

---

## 功能（v0.1.0）

### PDF 工具

拆分 · 合并 · 旋转 · 转换 · 压缩 · 水印

### 脱敏

1. **文件区**：添加文件；类型标签；按类型 / 按内容排序
2. **预览**：添加后自动加载；手动绘制 / 选择 / 删除脱敏框
3. **导出**：脱敏 PDF 或 PNG（不可逆）

**预览**：Ctrl + 滚轮缩放；可切换「脱敏效果」

---

## 技术栈

- Qt 6.11 + QML + C++
- PDF：qpdf + poppler
- OCR：**Tesseract** `chi_sim+eng`（`tools/tesseract/`，可离线）

---

## 快速开始

```bat
cd C:\Users\liu18\Documents\TechG\ProjectO
build.bat
run.bat
release.bat
```

首次使用 OCR：

```bat
setup-tesseract.bat
```

Kit：`Desktop Qt 6.11.1 MinGW 64-bit`

---

## 项目结构

```
ProjectO/
├── src/              # C++ 引擎（含 src/pdf/ PageCase 模块）
├── qml/              # 界面
├── tools/tesseract/  # 离线 OCR
├── docs/
├── build.bat / run.bat
└── README.md
```

构建产物：`build\Mosaic.exe` · 安装包：`dist\artifacts\Mosaic_0.1.0_win64_Setup.exe`

---

## 开发文档

| 文档 | 路径 |
|------|------|
| 设计说明 | [`docs/设计说明.md`](docs/设计说明.md) |
| Obsidian 开发记录 | `Obsidian\TechG\01_Notes\ProjectO-Mosaic-开发记录.md` |

---

## 产品定位

**Mosaic** — 本机 PDF 处理与文档脱敏，文件不出电脑。自动识别为辅助，导出前须人工复核。

**作者**：MeowYewy
