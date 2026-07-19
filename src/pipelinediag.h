#pragma once

#include <QString>

// Headless self-test: OCR + DOCX + PII rules. Returns empty on success, else error text.
QString runPipelineSelfTest();
