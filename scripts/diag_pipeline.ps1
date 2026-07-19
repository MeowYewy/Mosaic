# Quick offline diagnostic for Mosaic load/OCR/PII pipeline.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build'
$tess = Join-Path $build 'tools\tesseract\tesseract.exe'
$tessdata = Join-Path $build 'tools\tesseract\tessdata'
$exe = Join-Path $build 'Mosaic.exe'

Write-Host '=== Mosaic pipeline diagnostic ==='
Write-Host "build exe: $(Test-Path $exe) $exe"
Write-Host "tesseract: $(Test-Path $tess) $tess"
Write-Host "chi_sim:   $(Test-Path (Join-Path $tessdata 'chi_sim.traineddata'))"

# Create synthetic OCR test image
Add-Type -AssemblyName System.Drawing
$imgPath = Join-Path $build '_diag_ocr.png'
$bmp = New-Object System.Drawing.Bitmap 800, 240
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::White)
$font = New-Object System.Drawing.Font('Microsoft YaHei', 18)
$g.DrawString('姓名：张三', $font, [System.Drawing.Brushes]::Black, 20, 40)
$g.DrawString('手机：13812345678', $font, [System.Drawing.Brushes]::Black, 20, 90)
$g.DrawString('身份证：110101199001011234', $font, [System.Drawing.Brushes]::Black, 20, 140)
$bmp.Save($imgPath, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "test image: $imgPath"

Write-Host "`n--- Tesseract stdout ---"
& $tess $imgPath stdout -l chi_sim+eng --tessdata-dir $tessdata --psm 3

Write-Host "`n--- Tesseract TSV (first 8 lines) ---"
$tsvBase = Join-Path $build '_diag_out'
& $tess $imgPath $tsvBase -l chi_sim+eng --tessdata-dir $tessdata --psm 3 -c tessedit_create_tsv=1 | Out-Null
Get-Content ($tsvBase + '.tsv') -TotalCount 8

# Create minimal DOCX
$docxPath = Join-Path $build '_diag_test.docx'
$tmp = Join-Path $build '_diag_docx_tmp'
if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $tmp 'word') -Force | Out-Null
@'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:body>
    <w:p><w:r><w:rPr><w:b/></w:rPr><w:t>患者姓名：李四</w:t></w:r></w:p>
    <w:p><w:r><w:t>手机号：13987654321</w:t></w:r></w:p>
    <w:p><w:r><w:t>身份证号：11010119900303111X</w:t></w:r></w:p>
  </w:body>
</w:document>
'@ | Set-Content -Encoding UTF8 (Join-Path $tmp 'word\document.xml')
@'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>
'@ | Set-Content -Encoding UTF8 (Join-Path $tmp '[Content_Types].xml')
@'
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>
'@ | Set-Content -Encoding UTF8 (Join-Path $tmp '_rels\.rels')
if (Test-Path $docxPath) { Remove-Item $docxPath -Force }
Push-Location $tmp
tar -cf $docxPath *
Pop-Location
Write-Host "`n--- DOCX extract test ---"
$docxTmp = Join-Path $build '_diag_docx_unzip'
if (Test-Path $docxTmp) { Remove-Item $docxTmp -Recurse -Force }
New-Item -ItemType Directory -Path $docxTmp -Force | Out-Null
tar -xf $docxPath -C $docxTmp
$docXml = Join-Path $docxTmp 'word\document.xml'
Write-Host "document.xml exists: $(Test-Path $docXml)"
if (Test-Path $docXml) { Get-Content $docXml -TotalCount 6 }

Write-Host "`nDone. Test files:"
Write-Host "  $imgPath"
Write-Host "  $docxPath"
