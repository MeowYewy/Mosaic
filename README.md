# Mask Studio (ProjectO)



本地文档脱敏工具：导入 DOCX / PDF / PNG / JPEG，OCR 自动识别敏感信息并施加**不可逆**马赛克或色块，支持手动增删改标记。



<p align="center">

  <strong>本地 · 离线 · 人工复核</strong><br/>

  身份证 · 手机号 · 姓名 · 病历号

</p>



---



## 功能（v0.1.0）



1. **左侧文件区**：添加文件；类型标签；拖拽混排 / 按类型排序

2. **确认预览**：OCR（Tesseract 中英）+ 规则匹配 → 自动脱敏框

3. **手动标注**：绘制、选择、删除、改大小；色块 / 马赛克

4. **导出**：脱敏 PDF 或 PNG（不可逆）



**预览**：Ctrl + 滚轮缩放；可切换「脱敏效果」



---



## 技术栈



- Qt 6.11 + QML + C++

- PDF 预览：poppler（构建时从 ProjectP 复制）

- OCR：**Tesseract** `chi_sim+eng`（`tools/tesseract/`，可离线）



---



## 快速开始



```bat

D:\Qt\Tools\QtCreator\bin\qtcreator.exe D:\TechG\ProjectO\CMakeLists.txt



cd D:\TechG\ProjectO

build.bat

run.bat

release.bat

```



首次使用 OCR：



```bat

setup-tesseract.bat

```



Kit：`Desktop Qt 6.11.1 MinGW 64-bit` · 本机 Qt：`D:\Qt\6.11.1\mingw_64`（`scripts\env.bat` 亦支持 `C:\Qt`）



---



## 项目结构



```

ProjectO/

├── src/              # C++ 引擎、OCR、PII 检测

├── qml/              # 界面

├── tools/tesseract/  # 离线 OCR（chi_sim + eng）

├── docs/             # 设计说明

├── build.bat / run.bat

└── README.md

```



---



## 开发文档



| 文档 | 路径 |

|------|------|

| 详细开发记录 | `C:\Users\admin\Documents\TechG\01_Notes\ProjectO-MaskStudio-开发记录.md` |

| 进度汇报 | `C:\Users\admin\Documents\TechG\01_Notes\ProjectO-桌面端进度汇报.md` |

| Agent 快速回归 Skill | `C:\Users\admin\Documents\TechG\01_Notes\ProjectO-MaskStudio-Agent-Skill.md` |

| 设计说明 | [`docs/设计说明.md`](docs/设计说明.md) |

| 参考项目 | [ProjectP / PDF Studio](../ProjectP) |



---



## 产品定位



面向病历 / 报告等本地脱敏；文件不出电脑。**自动识别为辅助，导出前须人工复核。**



**作者**：MeowYewy


