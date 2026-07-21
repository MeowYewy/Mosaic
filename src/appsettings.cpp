#include "appsettings.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSettings>
#include <QStandardPaths>
#include <QCoreApplication>

AppSettings::AppSettings(QObject *parent)
    : QObject(parent)
{
    QSettings s;
    m_language = s.value(QStringLiteral("language"), QStringLiteral("zh_CN")).toString();
    m_theme = s.value(QStringLiteral("theme"), QStringLiteral("light")).toString();
    m_customFilePicker = s.value(QStringLiteral("customFilePicker"), true).toBool();
    m_modeTransition = s.value(QStringLiteral("modeTransition"), true).toBool();
}

void AppSettings::setLanguage(const QString &lang)
{
    if (m_language == lang)
        return;
    m_language = lang;
    QSettings().setValue(QStringLiteral("language"), m_language);
    ++m_languageRevision;
    emit languageRevisionChanged();
}

void AppSettings::setTheme(const QString &theme)
{
    if (m_theme == theme)
        return;
    m_theme = theme;
    QSettings().setValue(QStringLiteral("theme"), m_theme);
    ++m_themeRevision;
    emit themeRevisionChanged();
}

void AppSettings::setThemeBlend(qreal blend)
{
    const qreal clamped = qBound<qreal>(0.0, blend, 1.0);
    if (qFuzzyCompare(m_themeBlend, clamped))
        return;
    m_themeBlend = clamped;
    const bool mask = clamped >= 0.999;
    if (m_maskMode != mask) {
        m_maskMode = mask;
        emit maskModeChanged();
    }
    emit themeBlendChanged();
}

void AppSettings::setMaskMode(bool on)
{
    if (m_maskMode == on)
        return;
    m_maskMode = on;
    emit maskModeChanged();
}

void AppSettings::setCustomFilePicker(bool on)
{
    if (m_customFilePicker == on)
        return;
    m_customFilePicker = on;
    QSettings().setValue(QStringLiteral("customFilePicker"), m_customFilePicker);
    emit customFilePickerChanged();
}

void AppSettings::setModeTransition(bool on)
{
    if (m_modeTransition == on)
        return;
    m_modeTransition = on;
    QSettings().setValue(QStringLiteral("modeTransition"), m_modeTransition);
    emit modeTransitionChanged();
}

QString AppSettings::lastOutputDir() const
{
    return QSettings().value(QStringLiteral("lastOutputDir")).toString();
}

void AppSettings::rememberOutputPath(const QString &fileOrDir)
{
    const QString trimmed = fileOrDir.trimmed();
    if (trimmed.isEmpty())
        return;
    const QFileInfo info(trimmed);
    const QString dir = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    if (!dir.isEmpty())
        QSettings().setValue(QStringLiteral("lastOutputDir"), dir);
}

QStringList AppSettings::recentFiles() const
{
    return QSettings().value(QStringLiteral("recentFiles")).toStringList();
}

void AppSettings::rememberRecentFile(const QString &filePath)
{
    const QString trimmed = filePath.trimmed();
    if (trimmed.isEmpty())
        return;
    const QFileInfo info(trimmed);
    if (!info.isFile())
        return;
    const QString normalized = QDir::cleanPath(info.absoluteFilePath());
    QStringList list = QSettings().value(QStringLiteral("recentFiles")).toStringList();
    list.removeAll(normalized);
    list.prepend(normalized);
    while (list.size() > 20)
        list.removeLast();
    QSettings().setValue(QStringLiteral("recentFiles"), list);
}

void AppSettings::addWatermarkHistory(const QString &text)
{
    Q_UNUSED(text);
}

void AppSettings::removeWatermarkHistoryAt(int index)
{
    Q_UNUSED(index);
}

QStringList AppSettings::watermarkHistory() const
{
    return QSettings().value(QStringLiteral("watermarkHistory")).toStringList();
}

QString AppSettings::trKey(const QString &key) const
{
    static const QHash<QString, QHash<QString, QString>> table = {
        {QStringLiteral("appName"), {
            {QStringLiteral("zh_CN"), QStringLiteral("Mosaic")},
            {QStringLiteral("en"), QStringLiteral("Mosaic")},
        }},
        {QStringLiteral("pageCaseName"), {
            {QStringLiteral("zh_CN"), QStringLiteral("Mosaic")},
            {QStringLiteral("en"), QStringLiteral("Mosaic")},
        }},
        {QStringLiteral("tabSplit"), {
            {QStringLiteral("zh_CN"), QStringLiteral("拆分")},
            {QStringLiteral("en"), QStringLiteral("Split")},
        }},
        {QStringLiteral("tabMerge"), {
            {QStringLiteral("zh_CN"), QStringLiteral("合并")},
            {QStringLiteral("en"), QStringLiteral("Merge")},
        }},
        {QStringLiteral("tabRotate"), {
            {QStringLiteral("zh_CN"), QStringLiteral("旋转")},
            {QStringLiteral("en"), QStringLiteral("Rotate")},
        }},
        {QStringLiteral("tabConvert"), {
            {QStringLiteral("zh_CN"), QStringLiteral("转换")},
            {QStringLiteral("en"), QStringLiteral("Convert")},
        }},
        {QStringLiteral("tabCompress"), {
            {QStringLiteral("zh_CN"), QStringLiteral("压缩")},
            {QStringLiteral("en"), QStringLiteral("Compress")},
        }},
        {QStringLiteral("tabWatermark"), {
            {QStringLiteral("zh_CN"), QStringLiteral("水印")},
            {QStringLiteral("en"), QStringLiteral("Watermark")},
        }},
        {QStringLiteral("tabMask"), {
            {QStringLiteral("zh_CN"), QStringLiteral("脱敏")},
            {QStringLiteral("en"), QStringLiteral("Mask")},
        }},
        {QStringLiteral("tagline"), {
            {QStringLiteral("zh_CN"), QStringLiteral("本地文档处理与脱敏")},
            {QStringLiteral("en"), QStringLiteral("Local document toolkit and redaction")},
        }},
        {QStringLiteral("dropHint"), {
            {QStringLiteral("zh_CN"), QStringLiteral("拖放文件到此处或点击浏览文件")},
            {QStringLiteral("en"), QStringLiteral("Drop files here or click Browse")},
        }},
        {QStringLiteral("maskedPreviewReadOnly"), {
            {QStringLiteral("zh_CN"), QStringLiteral("脱敏预览（只读）— 基于退出脱敏模式前的最后设置")},
            {QStringLiteral("en"), QStringLiteral("Redacted preview (read-only) — last mask settings before exit")},
        }},
        {QStringLiteral("maskedPreviewLockedHint"), {
            {QStringLiteral("zh_CN"), QStringLiteral("脱敏预览中，无法添加或修改文件")},
            {QStringLiteral("en"), QStringLiteral("Redacted preview active; file list is locked")},
        }},
        {QStringLiteral("browse"), {
            {QStringLiteral("zh_CN"), QStringLiteral("浏览文件")},
            {QStringLiteral("en"), QStringLiteral("Browse")},
        }},
        {QStringLiteral("run"), {
            {QStringLiteral("zh_CN"), QStringLiteral("开始处理")},
            {QStringLiteral("en"), QStringLiteral("Start")},
        }},
        {QStringLiteral("splitDesc"), {
            {QStringLiteral("zh_CN"), QStringLiteral("将多页 PDF 拆分为单页")},
            {QStringLiteral("en"), QStringLiteral("Split PDF into single pages")},
        }},
        {QStringLiteral("mergeDesc"), {
            {QStringLiteral("zh_CN"), QStringLiteral("合并 PDF、Word、Markdown、图片等为单个 PDF")},
            {QStringLiteral("en"), QStringLiteral("Merge PDF, Word, Markdown, and images into one PDF")},
        }},
        {QStringLiteral("rotateDesc"), {
            {QStringLiteral("zh_CN"), QStringLiteral("旋转 PDF 页面")},
            {QStringLiteral("en"), QStringLiteral("Rotate PDF pages")},
        }},
        {QStringLiteral("convertDesc"), {
            {QStringLiteral("zh_CN"), QStringLiteral("在 PDF 与 Word、图片等格式间转换")},
            {QStringLiteral("en"), QStringLiteral("Convert between PDF and other formats")},
        }},
        {QStringLiteral("compressDesc"), {
            {QStringLiteral("zh_CN"), QStringLiteral("减小 PDF 文件体积")},
            {QStringLiteral("en"), QStringLiteral("Compress PDF file size")},
        }},
        {QStringLiteral("watermarkDesc"), {
            {QStringLiteral("zh_CN"), QStringLiteral("为 PDF 每一页添加文字水印")},
            {QStringLiteral("en"), QStringLiteral("Add text watermark to each page")},
        }},
        {QStringLiteral("noFilesEmpty"), {
            {QStringLiteral("zh_CN"), QStringLiteral("尚未添加文件")},
            {QStringLiteral("en"), QStringLiteral("No files added yet")},
        }},
        {QStringLiteral("fileList"), {
            {QStringLiteral("zh_CN"), QStringLiteral("文件列表")},
            {QStringLiteral("en"), QStringLiteral("Files")},
        }},
        {QStringLiteral("confirmPreview"), {
            {QStringLiteral("zh_CN"), QStringLiteral("确认并加载预览")},
            {QStringLiteral("en"), QStringLiteral("Confirm & Load Preview")},
        }},
        {QStringLiteral("export"), {
            {QStringLiteral("zh_CN"), QStringLiteral("导出脱敏文件")},
            {QStringLiteral("en"), QStringLiteral("Export")},
        }},
        {QStringLiteral("pdfPreview"), {
            {QStringLiteral("zh_CN"), QStringLiteral("文件预览")},
            {QStringLiteral("en"), QStringLiteral("Preview")},
        }},
        {QStringLiteral("maskPreview"), {
            {QStringLiteral("zh_CN"), QStringLiteral("脱敏预览")},
            {QStringLiteral("en"), QStringLiteral("Redacted preview")},
        }},
        {QStringLiteral("manualHint"), {
            {QStringLiteral("zh_CN"), QStringLiteral("←/Q上一页 · →/E下一页 · R全览 · Ctrl+滚轮缩放 · 1绘制 2穿透绘制 3选择 4删除 · Enter脱敏 · Tab样式 · 穿透标记作用于当前文件全部页")},
            {QStringLiteral("en"), QStringLiteral("←/Q · →/E · R fit · Ctrl+wheel · 1 draw · 2 fixed · 3 select · 4 delete · Enter masked · Tab style · fixed marks apply to all pages of the file")},
        }},
        {QStringLiteral("maskShortcutTooltip"), {
            {QStringLiteral("zh_CN"), QStringLiteral("上一页      Q / ←\n下一页      E / →\n全览        R\n缩放        Ctrl + 滚轮\n移动视图    滚轮 / 右键拖拽\n绘制        1\n穿透绘制    2\n选择        3\n删除        4\n脱敏预览    Enter\n切换样式    Tab")},
            {QStringLiteral("en"), QStringLiteral("Previous page   Q / ←\nNext page       E / →\nFit view         R\nZoom            Ctrl + wheel\nPan             Wheel / right-drag\nDraw            1\nFixed mark      2\nSelect          3\nDelete          4\nMasked preview  Enter\nToggle style    Tab")},
        }},
        {QStringLiteral("maskShortcutTooltipMasked"), {
            {QStringLiteral("zh_CN"), QStringLiteral("全览        R\n切换样式    Tab\n返回编辑    Enter\n删除选中    Delete")},
            {QStringLiteral("en"), QStringLiteral("Fit view        R\nToggle style    Tab\nBack to edit    Enter\nDelete mark     Delete")},
        }},
        {QStringLiteral("shortcutPrevPage"), {
            {QStringLiteral("zh_CN"), QStringLiteral("上一页")},
            {QStringLiteral("en"), QStringLiteral("Previous page")},
        }},
        {QStringLiteral("shortcutNextPage"), {
            {QStringLiteral("zh_CN"), QStringLiteral("下一页")},
            {QStringLiteral("en"), QStringLiteral("Next page")},
        }},
        {QStringLiteral("shortcutZoom"), {
            {QStringLiteral("zh_CN"), QStringLiteral("缩放")},
            {QStringLiteral("en"), QStringLiteral("Zoom")},
        }},
        {QStringLiteral("shortcutPan"), {
            {QStringLiteral("zh_CN"), QStringLiteral("移动视图")},
            {QStringLiteral("en"), QStringLiteral("Pan")},
        }},
        {QStringLiteral("shortcutFitView"), {
            {QStringLiteral("zh_CN"), QStringLiteral("全览")},
            {QStringLiteral("en"), QStringLiteral("Fit view")},
        }},
        {QStringLiteral("shortcutDraw"), {
            {QStringLiteral("zh_CN"), QStringLiteral("绘制")},
            {QStringLiteral("en"), QStringLiteral("Draw")},
        }},
        {QStringLiteral("shortcutSelect"), {
            {QStringLiteral("zh_CN"), QStringLiteral("选择")},
            {QStringLiteral("en"), QStringLiteral("Select")},
        }},
        {QStringLiteral("shortcutFixedDraw"), {
            {QStringLiteral("zh_CN"), QStringLiteral("穿透绘制")},
            {QStringLiteral("en"), QStringLiteral("Fixed mark")},
        }},
        {QStringLiteral("shortcutDeleteMark"), {
            {QStringLiteral("zh_CN"), QStringLiteral("删除")},
            {QStringLiteral("en"), QStringLiteral("Delete")},
        }},
        {QStringLiteral("shortcutMaskedPreview"), {
            {QStringLiteral("zh_CN"), QStringLiteral("脱敏预览")},
            {QStringLiteral("en"), QStringLiteral("Masked preview")},
        }},
        {QStringLiteral("shortcutToggleStyle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("切换样式")},
            {QStringLiteral("en"), QStringLiteral("Toggle style")},
        }},
        {QStringLiteral("shortcutBackEdit"), {
            {QStringLiteral("zh_CN"), QStringLiteral("返回编辑")},
            {QStringLiteral("en"), QStringLiteral("Back to edit")},
        }},
        {QStringLiteral("shortcutDeleteSelection"), {
            {QStringLiteral("zh_CN"), QStringLiteral("删除选中")},
            {QStringLiteral("en"), QStringLiteral("Delete mark")},
        }},
        {QStringLiteral("shortcutZoomKeys"), {
            {QStringLiteral("zh_CN"), QStringLiteral("Ctrl + 滚轮")},
            {QStringLiteral("en"), QStringLiteral("Ctrl + wheel")},
        }},
        {QStringLiteral("shortcutPanKeys"), {
            {QStringLiteral("zh_CN"), QStringLiteral("滚轮 / 右键拖拽")},
            {QStringLiteral("en"), QStringLiteral("Wheel / right-drag")},
        }},
        {QStringLiteral("zoomHint"), {
            {QStringLiteral("zh_CN"), QStringLiteral("Ctrl+滚轮缩放 · 滚轮移动 · 右键拖拽")},
            {QStringLiteral("en"), QStringLiteral("Ctrl+wheel · wheel pan · R-drag")},
        }},
        {QStringLiteral("showMaskPreview"), {
            {QStringLiteral("zh_CN"), QStringLiteral("脱敏效果")},
            {QStringLiteral("en"), QStringLiteral("Masked view")},
        }},
        {QStringLiteral("autoMarks"), {
            {QStringLiteral("zh_CN"), QStringLiteral("自动标记")},
            {QStringLiteral("en"), QStringLiteral("Auto marks")},
        }},
        {QStringLiteral("ocrUnavailable"), {
            {QStringLiteral("zh_CN"), QStringLiteral("未检测到 OCR，扫描图请安装 Tesseract")},
            {QStringLiteral("en"), QStringLiteral("OCR not found — install Tesseract for scans")},
        }},
        {QStringLiteral("toolDraw"), {
            {QStringLiteral("zh_CN"), QStringLiteral("绘制")},
            {QStringLiteral("en"), QStringLiteral("Draw")},
        }},
        {QStringLiteral("toolFixedDraw"), {
            {QStringLiteral("zh_CN"), QStringLiteral("穿透绘制")},
            {QStringLiteral("en"), QStringLiteral("Fixed mark")},
        }},
        {QStringLiteral("toolSelect"), {
            {QStringLiteral("zh_CN"), QStringLiteral("选择")},
            {QStringLiteral("en"), QStringLiteral("Select")},
        }},
        {QStringLiteral("deleteMark"), {
            {QStringLiteral("zh_CN"), QStringLiteral("删除")},
            {QStringLiteral("en"), QStringLiteral("Delete")},
        }},
        {QStringLiteral("toolFitView"), {
            {QStringLiteral("zh_CN"), QStringLiteral("全览")},
            {QStringLiteral("en"), QStringLiteral("Fit view")},
        }},
        {QStringLiteral("mosaicStyle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("脱敏样式")},
            {QStringLiteral("en"), QStringLiteral("Style")},
        }},
        {QStringLiteral("styleBlock"), {
            {QStringLiteral("zh_CN"), QStringLiteral("色块")},
            {QStringLiteral("en"), QStringLiteral("Block")},
        }},
        {QStringLiteral("stylePixel"), {
            {QStringLiteral("zh_CN"), QStringLiteral("马赛克")},
            {QStringLiteral("en"), QStringLiteral("Pixelate")},
        }},
        {QStringLiteral("sortMode"), {
            {QStringLiteral("zh_CN"), QStringLiteral("排序")},
            {QStringLiteral("en"), QStringLiteral("Sort")},
        }},
        {QStringLiteral("sortFree"), {
            {QStringLiteral("zh_CN"), QStringLiteral("自由混排")},
            {QStringLiteral("en"), QStringLiteral("Free")},
        }},
        {QStringLiteral("sortByType"), {
            {QStringLiteral("zh_CN"), QStringLiteral("按类型")},
            {QStringLiteral("en"), QStringLiteral("By Type")},
        }},
        {QStringLiteral("sortByContent"), {
            {QStringLiteral("zh_CN"), QStringLiteral("按内容")},
            {QStringLiteral("en"), QStringLiteral("By Content")},
        }},
        {QStringLiteral("contentSortAnalyzing"), {
            {QStringLiteral("zh_CN"), QStringLiteral("正在读取文件内容…")},
            {QStringLiteral("en"), QStringLiteral("Reading file content…")},
        }},
        {QStringLiteral("clear"), {
            {QStringLiteral("zh_CN"), QStringLiteral("清空")},
            {QStringLiteral("en"), QStringLiteral("Clear")},
        }},
        {QStringLiteral("maskedLocked"), {
            {QStringLiteral("zh_CN"), QStringLiteral("预览脱敏效果中 · Tab切换样式 · Enter返回编辑 · Delete删除选中")},
            {QStringLiteral("en"), QStringLiteral("Masked view · Tab style · Enter edit · Delete selection")},
        }},
        {QStringLiteral("about"), {
            {QStringLiteral("zh_CN"), QStringLiteral("关于")},
            {QStringLiteral("en"), QStringLiteral("About")},
        }},
        {QStringLiteral("checkUpdate"), {
            {QStringLiteral("zh_CN"), QStringLiteral("检查更新")},
            {QStringLiteral("en"), QStringLiteral("Check for Updates")},
        }},
        {QStringLiteral("newVersion"), {
            {QStringLiteral("zh_CN"), QStringLiteral("新版本")},
            {QStringLiteral("en"), QStringLiteral("New")},
        }},
        {QStringLiteral("installUpdate"), {
            {QStringLiteral("zh_CN"), QStringLiteral("安装")},
            {QStringLiteral("en"), QStringLiteral("Install")},
        }},
        {QStringLiteral("installConfirmTitle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("安装更新")},
            {QStringLiteral("en"), QStringLiteral("Install update")},
        }},
        {QStringLiteral("installConfirmMessage"), {
            {QStringLiteral("zh_CN"), QStringLiteral("未保存的内容将会丢失，是否继续？")},
            {QStringLiteral("en"), QStringLiteral("Unsaved changes will be lost. Continue?")},
        }},
        {QStringLiteral("pickerCancel"), {
            {QStringLiteral("zh_CN"), QStringLiteral("取消")},
            {QStringLiteral("en"), QStringLiteral("Cancel")},
        }},
        {QStringLiteral("back"), {
            {QStringLiteral("zh_CN"), QStringLiteral("返回")},
            {QStringLiteral("en"), QStringLiteral("Back")},
        }},
        {QStringLiteral("addedFiles"), {
            {QStringLiteral("zh_CN"), QStringLiteral("已添加")},
            {QStringLiteral("en"), QStringLiteral("Added")},
        }},
        {QStringLiteral("success"), {
            {QStringLiteral("zh_CN"), QStringLiteral("已完成")},
            {QStringLiteral("en"), QStringLiteral("Done")},
        }},
        {QStringLiteral("failed"), {
            {QStringLiteral("zh_CN"), QStringLiteral("处理失败")},
            {QStringLiteral("en"), QStringLiteral("Failed")},
        }},
        {QStringLiteral("pageRangeInvalid"), {
            {QStringLiteral("zh_CN"), QStringLiteral("页码格式无效")},
            {QStringLiteral("en"), QStringLiteral("Invalid page range")},
        }},
        {QStringLiteral("needPdf"), {
            {QStringLiteral("zh_CN"), QStringLiteral("该文件类型不支持此操作")},
            {QStringLiteral("en"), QStringLiteral("This file type is not supported")},
        }},
        {QStringLiteral("watermarkPlaceholder"), {
            {QStringLiteral("zh_CN"), QStringLiteral("水印文字")},
            {QStringLiteral("en"), QStringLiteral("Watermark text")},
        }},
        {QStringLiteral("compressLow"), {
            {QStringLiteral("zh_CN"), QStringLiteral("标准")},
            {QStringLiteral("en"), QStringLiteral("Standard")},
        }},
        {QStringLiteral("compressMid"), {
            {QStringLiteral("zh_CN"), QStringLiteral("较高")},
            {QStringLiteral("en"), QStringLiteral("High")},
        }},
        {QStringLiteral("compressHigh"), {
            {QStringLiteral("zh_CN"), QStringLiteral("最高")},
            {QStringLiteral("en"), QStringLiteral("Maximum")},
        }},
        {QStringLiteral("formatPdf"), {
            {QStringLiteral("zh_CN"), QStringLiteral("PDF")},
            {QStringLiteral("en"), QStringLiteral("PDF")},
        }},
        {QStringLiteral("formatPng"), {
            {QStringLiteral("zh_CN"), QStringLiteral("PNG")},
            {QStringLiteral("en"), QStringLiteral("PNG")},
        }},
        {QStringLiteral("formatJpeg"), {
            {QStringLiteral("zh_CN"), QStringLiteral("JPEG")},
            {QStringLiteral("en"), QStringLiteral("JPEG")},
        }},
        {QStringLiteral("formatWord"), {
            {QStringLiteral("zh_CN"), QStringLiteral("Word")},
            {QStringLiteral("en"), QStringLiteral("Word")},
        }},
        {QStringLiteral("splitNumLower"), {
            {QStringLiteral("zh_CN"), QStringLiteral("一")},
            {QStringLiteral("en"), QStringLiteral("a")},
        }},
        {QStringLiteral("splitNumUpper"), {
            {QStringLiteral("zh_CN"), QStringLiteral("壹")},
            {QStringLiteral("en"), QStringLiteral("A")},
        }},
        {QStringLiteral("aboutTitle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("关于 Mosaic")},
            {QStringLiteral("en"), QStringLiteral("About Mosaic")},
        }},
        {QStringLiteral("pageCaseTagline"), {
            {QStringLiteral("zh_CN"), QStringLiteral("本地文档处理与脱敏")},
            {QStringLiteral("en"), QStringLiteral("Local document toolkit and redaction")},
        }},
        {QStringLiteral("aboutTagline"), {
            {QStringLiteral("zh_CN"), QStringLiteral("本地文档处理与脱敏")},
            {QStringLiteral("en"), QStringLiteral("Local document toolkit and redaction")},
        }},
        {QStringLiteral("aboutCopyright"), {
            {QStringLiteral("zh_CN"), QStringLiteral("版权所有")},
            {QStringLiteral("en"), QStringLiteral("All rights reserved")},
        }},
        {QStringLiteral("devDisclaimer"), {
            {QStringLiteral("zh_CN"), QStringLiteral("开发中内容，不代表最终品质。")},
            {QStringLiteral("en"), QStringLiteral("Work in progress; not final quality.")},
        }},
        {QStringLiteral("changelog"), {
            {QStringLiteral("zh_CN"), QStringLiteral("更新日志")},
            {QStringLiteral("en"), QStringLiteral("Changelog")},
        }},
        {QStringLiteral("changelogTitle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("更新日志")},
            {QStringLiteral("en"), QStringLiteral("Changelog")},
        }},
        {QStringLiteral("changelogEmpty"), {
            {QStringLiteral("zh_CN"), QStringLiteral("暂无更新记录")},
            {QStringLiteral("en"), QStringLiteral("No changelog entries yet")},
        }},
        {QStringLiteral("pickerOpenTitle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("选择文件")},
            {QStringLiteral("en"), QStringLiteral("Open files")},
        }},
        {QStringLiteral("pickerSaveTitle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("保存文件")},
            {QStringLiteral("en"), QStringLiteral("Save file")},
        }},
        {QStringLiteral("pickerFolderTitle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("选择文件夹")},
            {QStringLiteral("en"), QStringLiteral("Select folder")},
        }},
        {QStringLiteral("pickerOpen"), {
            {QStringLiteral("zh_CN"), QStringLiteral("打开")},
            {QStringLiteral("en"), QStringLiteral("Open")},
        }},
        {QStringLiteral("pickerSave"), {
            {QStringLiteral("zh_CN"), QStringLiteral("保存")},
            {QStringLiteral("en"), QStringLiteral("Save")},
        }},
        {QStringLiteral("pickerSelectFolder"), {
            {QStringLiteral("zh_CN"), QStringLiteral("选择")},
            {QStringLiteral("en"), QStringLiteral("Select")},
        }},
        {QStringLiteral("pickerFileName"), {
            {QStringLiteral("zh_CN"), QStringLiteral("文件名")},
            {QStringLiteral("en"), QStringLiteral("File name")},
        }},
        {QStringLiteral("pickerFileType"), {
            {QStringLiteral("zh_CN"), QStringLiteral("文件类型")},
            {QStringLiteral("en"), QStringLiteral("File type")},
        }},
        {QStringLiteral("pickerName"), {
            {QStringLiteral("zh_CN"), QStringLiteral("名称")},
            {QStringLiteral("en"), QStringLiteral("Name")},
        }},
        {QStringLiteral("pickerSize"), {
            {QStringLiteral("zh_CN"), QStringLiteral("大小")},
            {QStringLiteral("en"), QStringLiteral("Size")},
        }},
        {QStringLiteral("pickerModified"), {
            {QStringLiteral("zh_CN"), QStringLiteral("修改时间")},
            {QStringLiteral("en"), QStringLiteral("Modified")},
        }},
        {QStringLiteral("pickerNewFolder"), {
            {QStringLiteral("zh_CN"), QStringLiteral("新建文件夹")},
            {QStringLiteral("en"), QStringLiteral("New folder")},
        }},
        {QStringLiteral("pickerShowHidden"), {
            {QStringLiteral("zh_CN"), QStringLiteral("显示隐藏项")},
            {QStringLiteral("en"), QStringLiteral("Show hidden items")},
        }},
        {QStringLiteral("pickerUp"), {
            {QStringLiteral("zh_CN"), QStringLiteral("上一级")},
            {QStringLiteral("en"), QStringLiteral("Up")},
        }},
        {QStringLiteral("pickerRefresh"), {
            {QStringLiteral("zh_CN"), QStringLiteral("刷新")},
            {QStringLiteral("en"), QStringLiteral("Refresh")},
        }},
        {QStringLiteral("pickerPlaceHome"), {
            {QStringLiteral("zh_CN"), QStringLiteral("当前")},
            {QStringLiteral("en"), QStringLiteral("Current")},
        }},
        {QStringLiteral("pickerPlaceDesktop"), {
            {QStringLiteral("zh_CN"), QStringLiteral("桌面")},
            {QStringLiteral("en"), QStringLiteral("Desktop")},
        }},
        {QStringLiteral("pickerPlaceDocuments"), {
            {QStringLiteral("zh_CN"), QStringLiteral("文档")},
            {QStringLiteral("en"), QStringLiteral("Documents")},
        }},
        {QStringLiteral("pickerPlaceDownloads"), {
            {QStringLiteral("zh_CN"), QStringLiteral("下载")},
            {QStringLiteral("en"), QStringLiteral("Downloads")},
        }},
        {QStringLiteral("pickerPlaceComputer"), {
            {QStringLiteral("zh_CN"), QStringLiteral("此电脑")},
            {QStringLiteral("en"), QStringLiteral("This PC")},
        }},
        {QStringLiteral("pickerPathMissing"), {
            {QStringLiteral("zh_CN"), QStringLiteral("无法访问此路径")},
            {QStringLiteral("en"), QStringLiteral("Cannot access this path")},
        }},
        {QStringLiteral("pickerInvalidFolderName"), {
            {QStringLiteral("zh_CN"), QStringLiteral("文件夹名称无效")},
            {QStringLiteral("en"), QStringLiteral("Invalid folder name")},
        }},
        {QStringLiteral("pickerCreateFolderFailed"), {
            {QStringLiteral("zh_CN"), QStringLiteral("无法创建文件夹")},
            {QStringLiteral("en"), QStringLiteral("Could not create folder")},
        }},
        {QStringLiteral("pickerFolderExistsTitle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("名称已存在")},
            {QStringLiteral("en"), QStringLiteral("Name already exists")},
        }},
        {QStringLiteral("pickerFolderExistsMessage"), {
            {QStringLiteral("zh_CN"), QStringLiteral("当前位置已有同名文件夹")},
            {QStringLiteral("en"), QStringLiteral("A folder with this name already exists here")},
        }},
        {QStringLiteral("pickerFileExistsMessage"), {
            {QStringLiteral("zh_CN"), QStringLiteral("当前位置已有同名文件")},
            {QStringLiteral("en"), QStringLiteral("A file with this name already exists here")},
        }},
        {QStringLiteral("pickerOk"), {
            {QStringLiteral("zh_CN"), QStringLiteral("确定")},
            {QStringLiteral("en"), QStringLiteral("OK")},
        }},
        {QStringLiteral("pickerCopy"), {
            {QStringLiteral("zh_CN"), QStringLiteral("复制")},
            {QStringLiteral("en"), QStringLiteral("Copy")},
        }},
        {QStringLiteral("pickerCut"), {
            {QStringLiteral("zh_CN"), QStringLiteral("剪切")},
            {QStringLiteral("en"), QStringLiteral("Cut")},
        }},
        {QStringLiteral("pickerPaste"), {
            {QStringLiteral("zh_CN"), QStringLiteral("粘贴")},
            {QStringLiteral("en"), QStringLiteral("Paste")},
        }},
        {QStringLiteral("pickerRename"), {
            {QStringLiteral("zh_CN"), QStringLiteral("重命名")},
            {QStringLiteral("en"), QStringLiteral("Rename")},
        }},
        {QStringLiteral("pickerDelete"), {
            {QStringLiteral("zh_CN"), QStringLiteral("删除")},
            {QStringLiteral("en"), QStringLiteral("Delete")},
        }},
        {QStringLiteral("pickerDeleteTitle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("确认删除")},
            {QStringLiteral("en"), QStringLiteral("Confirm delete")},
        }},
        {QStringLiteral("pickerDeleteMessageOne"), {
            {QStringLiteral("zh_CN"), QStringLiteral("确定将「%1」移到回收站吗？")},
            {QStringLiteral("en"), QStringLiteral("Move \"%1\" to the Recycle Bin?")},
        }},
        {QStringLiteral("pickerDeleteMessageMany"), {
            {QStringLiteral("zh_CN"), QStringLiteral("确定将选中的 %1 个项目移到回收站吗？")},
            {QStringLiteral("en"), QStringLiteral("Move %1 selected items to the Recycle Bin?")},
        }},
        {QStringLiteral("pickerNameExistsMessage"), {
            {QStringLiteral("zh_CN"), QStringLiteral("当前位置已有同名文件或文件夹")},
            {QStringLiteral("en"), QStringLiteral("An item with this name already exists here")},
        }},
        {QStringLiteral("pickerPasteFailed"), {
            {QStringLiteral("zh_CN"), QStringLiteral("部分内容无法粘贴")},
            {QStringLiteral("en"), QStringLiteral("Some items could not be pasted")},
        }},
        {QStringLiteral("pickerRenameFailed"), {
            {QStringLiteral("zh_CN"), QStringLiteral("无法重命名")},
            {QStringLiteral("en"), QStringLiteral("Could not rename")},
        }},
        {QStringLiteral("pickerDeleteFailed"), {
            {QStringLiteral("zh_CN"), QStringLiteral("部分内容无法删除")},
            {QStringLiteral("en"), QStringLiteral("Some items could not be deleted")},
        }},
        {QStringLiteral("pickerSearch"), {
            {QStringLiteral("zh_CN"), QStringLiteral("搜索当前目录")},
            {QStringLiteral("en"), QStringLiteral("Search this folder")},
        }},
        {QStringLiteral("pickerOverwriteTitle"), {
            {QStringLiteral("zh_CN"), QStringLiteral("文件已存在")},
            {QStringLiteral("en"), QStringLiteral("File already exists")},
        }},
        {QStringLiteral("pickerOverwriteMessage"), {
            {QStringLiteral("zh_CN"), QStringLiteral("目标位置已有同名文件，要替换吗？")},
            {QStringLiteral("en"), QStringLiteral("Replace the existing file?")},
        }},
        {QStringLiteral("pickerNewFolderName"), {
            {QStringLiteral("zh_CN"), QStringLiteral("新文件夹名称")},
            {QStringLiteral("en"), QStringLiteral("New folder name")},
        }},
        {QStringLiteral("pickerEmpty"), {
            {QStringLiteral("zh_CN"), QStringLiteral("此文件夹为空")},
            {QStringLiteral("en"), QStringLiteral("This folder is empty")},
        }},
        {QStringLiteral("pickerFilterAll"), {
            {QStringLiteral("zh_CN"), QStringLiteral("所有文件")},
            {QStringLiteral("en"), QStringLiteral("All files")},
        }},
        {QStringLiteral("pickerFilterRecent"), {
            {QStringLiteral("zh_CN"), QStringLiteral("最近打开")},
            {QStringLiteral("en"), QStringLiteral("Recent")},
        }},
        {QStringLiteral("pickerFilterPdf"), {
            {QStringLiteral("zh_CN"), QStringLiteral("PDF 文档")},
            {QStringLiteral("en"), QStringLiteral("PDF documents")},
        }},
        {QStringLiteral("pickerFilterOffice"), {
            {QStringLiteral("zh_CN"), QStringLiteral("Office 文档")},
            {QStringLiteral("en"), QStringLiteral("Office documents")},
        }},
        {QStringLiteral("pickerFilterImages"), {
            {QStringLiteral("zh_CN"), QStringLiteral("图片")},
            {QStringLiteral("en"), QStringLiteral("Images")},
        }},
        {QStringLiteral("pickerFilterText"), {
            {QStringLiteral("zh_CN"), QStringLiteral("文本文件")},
            {QStringLiteral("en"), QStringLiteral("Text files")},
        }},
        {QStringLiteral("pickerRecentEmpty"), {
            {QStringLiteral("zh_CN"), QStringLiteral("暂无最近打开的文件")},
            {QStringLiteral("en"), QStringLiteral("No recent files")},
        }},
        {QStringLiteral("theme"), {
            {QStringLiteral("zh_CN"), QStringLiteral("界面")},
            {QStringLiteral("en"), QStringLiteral("Theme")},
        }},
        {QStringLiteral("language"), {
            {QStringLiteral("zh_CN"), QStringLiteral("语言")},
            {QStringLiteral("en"), QStringLiteral("Language")},
        }},
        {QStringLiteral("light"), {
            {QStringLiteral("zh_CN"), QStringLiteral("浅色")},
            {QStringLiteral("en"), QStringLiteral("Light")},
        }},
        {QStringLiteral("dark"), {
            {QStringLiteral("zh_CN"), QStringLiteral("深色")},
            {QStringLiteral("en"), QStringLiteral("Dark")},
        }},
        {QStringLiteral("close"), {
            {QStringLiteral("zh_CN"), QStringLiteral("关闭")},
            {QStringLiteral("en"), QStringLiteral("Close")},
        }},
        {QStringLiteral("settings"), {
            {QStringLiteral("zh_CN"), QStringLiteral("设置")},
            {QStringLiteral("en"), QStringLiteral("Settings")},
        }},
        {QStringLiteral("settingOn"), {
            {QStringLiteral("zh_CN"), QStringLiteral("开启")},
            {QStringLiteral("en"), QStringLiteral("On")},
        }},
        {QStringLiteral("settingOff"), {
            {QStringLiteral("zh_CN"), QStringLiteral("关闭")},
            {QStringLiteral("en"), QStringLiteral("Off")},
        }},
        {QStringLiteral("settingCustomPicker"), {
            {QStringLiteral("zh_CN"), QStringLiteral("风格化文件选择")},
            {QStringLiteral("en"), QStringLiteral("Styled file picker")},
        }},
        {QStringLiteral("settingModeTransition"), {
            {QStringLiteral("zh_CN"), QStringLiteral("切换模式动画")},
            {QStringLiteral("en"), QStringLiteral("Mode transition")},
        }},
        {QStringLiteral("filesAdded"), {
            {QStringLiteral("zh_CN"), QStringLiteral("个文件已添加")},
            {QStringLiteral("en"), QStringLiteral("file(s) added")},
        }},
        {QStringLiteral("emptyPreview"), {
            {QStringLiteral("zh_CN"), QStringLiteral("添加或浏览文件即可预览")},
            {QStringLiteral("en"), QStringLiteral("Add or browse files to preview")},
        }},
        {QStringLiteral("loadingPreview"), {
            {QStringLiteral("zh_CN"), QStringLiteral("正在加载预览…")},
            {QStringLiteral("en"), QStringLiteral("Loading preview…")},
        }},
        {QStringLiteral("exporting"), {
            {QStringLiteral("zh_CN"), QStringLiteral("正在导出…")},
            {QStringLiteral("en"), QStringLiteral("Exporting…")},
        }},
        {QStringLiteral("preparingMaskedPreview"), {
            {QStringLiteral("zh_CN"), QStringLiteral("正在生成脱敏预览…")},
            {QStringLiteral("en"), QStringLiteral("Preparing redacted preview…")},
        }},
        {QStringLiteral("detecting"), {
            {QStringLiteral("zh_CN"), QStringLiteral("正在加载文件…")},
            {QStringLiteral("en"), QStringLiteral("Loading files…")},
        }},
        {QStringLiteral("pageLoading"), {
            {QStringLiteral("zh_CN"), QStringLiteral("正在渲染本页…")},
            {QStringLiteral("en"), QStringLiteral("Rendering page…")},
        }},
        {QStringLiteral("done"), {
            {QStringLiteral("zh_CN"), QStringLiteral("已完成")},
            {QStringLiteral("en"), QStringLiteral("Done")},
        }},
        {QStringLiteral("exportOk"), {
            {QStringLiteral("zh_CN"), QStringLiteral("脱敏文件已导出")},
            {QStringLiteral("en"), QStringLiteral("Exported")},
        }},
        {QStringLiteral("noFilesRequired"), {
            {QStringLiteral("zh_CN"), QStringLiteral("请先添加文件")},
            {QStringLiteral("en"), QStringLiteral("Add files first")},
        }},
        {QStringLiteral("version"), {
            {QStringLiteral("zh_CN"), QStringLiteral("版本")},
            {QStringLiteral("en"), QStringLiteral("Version")},
        }},
        {QStringLiteral("author"), {
            {QStringLiteral("zh_CN"), QStringLiteral("MeowYewy")},
            {QStringLiteral("en"), QStringLiteral("MeowYewy")},
        }},
    };

    const auto it = table.constFind(key);
    if (it == table.cend())
        return key;
    const auto &map = *it;
    if (map.contains(m_language))
        return map.value(m_language);
    return map.value(QStringLiteral("zh_CN"), key);
}
