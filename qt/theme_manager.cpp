#include "theme_manager.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDebug>

ThemeManager& ThemeManager::instance() {
    static ThemeManager instance;
    return instance;
}

ThemeManager::ThemeManager() {
    // Load default dark theme
    ThemeColors darkTheme;
    darkTheme.window = QColor(37, 37, 38);
    darkTheme.windowText = Qt::white;
    darkTheme.base = QColor(30, 30, 30);
    darkTheme.alternateBase = QColor(45, 45, 48);
    darkTheme.toolTipBase = Qt::white;
    darkTheme.toolTipText = Qt::white;
    darkTheme.text = Qt::white;
    darkTheme.button = QColor(45, 45, 48);
    darkTheme.buttonText = Qt::white;
    darkTheme.brightText = Qt::red;
    darkTheme.link = QColor(90, 160, 255);
    darkTheme.highlight = QColor(90, 160, 255);
    darkTheme.highlightedText = Qt::black;

    m_themes["Default Dark"] = darkTheme;
    m_currentTheme = "Default Dark";
}

ThemeManager::~ThemeManager() {
}

QColor ThemeManager::parseUnrealColor(const QString& colorString) {
    // Parse Unreal Engine color format: "(R=0.123456,G=0.234567,B=0.345678,A=1.000000)"
    QRegularExpression regex("\\(R=([0-9.]+),G=([0-9.]+),B=([0-9.]+),A=([0-9.]+)\\)");
    QRegularExpressionMatch match = regex.match(colorString);

    if (match.hasMatch()) {
        float r = match.captured(1).toFloat();
        float g = match.captured(2).toFloat();
        float b = match.captured(3).toFloat();
        float a = match.captured(4).toFloat();

        return QColor::fromRgbF(r, g, b, a);
    }

    return Qt::black;
}

void ThemeManager::loadThemesFromDirectory(const QString& directory) {
    QDir dir(directory);
    QStringList filters;
    filters << "*.json";
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files | QDir::Readable);

    for (const QFileInfo& fileInfo : files) {
        QFile file(fileInfo.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly)) {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);

            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                ThemeColors colors = parseThemeJson(doc);
                QString themeName = doc.object().value("DisplayName").toString();
                if (themeName.isEmpty()) {
                    themeName = fileInfo.baseName();
                }
                m_themes[themeName] = colors;
            }
            file.close();
        }
    }
}

ThemeColors ThemeManager::parseThemeJson(const QJsonDocument& doc) {
    ThemeColors colors;

    QJsonObject root = doc.object();
    QJsonObject colorsObj = root.value("Colors").toObject();

    // Map Unreal Engine style colors to Qt palette colors
    colors.window = parseUnrealColor(colorsObj.value("EStyleColor::Background").toString());
    colors.windowText = parseUnrealColor(colorsObj.value("EStyleColor::Foreground").toString());
    colors.base = parseUnrealColor(colorsObj.value("EStyleColor::Input").toString());
    colors.alternateBase = parseUnrealColor(colorsObj.value("EStyleColor::Recessed").toString());
    colors.toolTipBase = parseUnrealColor(colorsObj.value("EStyleColor::Background").toString());
    colors.toolTipText = parseUnrealColor(colorsObj.value("EStyleColor::Foreground").toString());
    colors.text = parseUnrealColor(colorsObj.value("EStyleColor::Foreground").toString());
    colors.button = parseUnrealColor(colorsObj.value("EStyleColor::Panel").toString());
    colors.buttonText = parseUnrealColor(colorsObj.value("EStyleColor::Foreground").toString());
    colors.brightText = parseUnrealColor(colorsObj.value("EStyleColor::Warning").toString());
    colors.link = parseUnrealColor(colorsObj.value("EStyleColor::AccentBlue").toString());
    colors.highlight = parseUnrealColor(colorsObj.value("EStyleColor::Select").toString());
    colors.highlightedText = parseUnrealColor(colorsObj.value("EStyleColor::ForegroundInverted").toString());

    return colors;
}

QPalette ThemeManager::createPaletteFromTheme(const ThemeColors& colors) {
    QPalette palette;

    palette.setColor(QPalette::Window, colors.window);
    palette.setColor(QPalette::WindowText, colors.windowText);
    palette.setColor(QPalette::Base, colors.base);
    palette.setColor(QPalette::AlternateBase, colors.alternateBase);
    palette.setColor(QPalette::ToolTipBase, colors.toolTipBase);
    palette.setColor(QPalette::ToolTipText, colors.toolTipText);
    palette.setColor(QPalette::Text, colors.text);
    palette.setColor(QPalette::Button, colors.button);
    palette.setColor(QPalette::ButtonText, colors.buttonText);
    palette.setColor(QPalette::BrightText, colors.brightText);
    palette.setColor(QPalette::Link, colors.link);
    palette.setColor(QPalette::Highlight, colors.highlight);
    palette.setColor(QPalette::HighlightedText, colors.highlightedText);

    return palette;
}

void ThemeManager::applyTheme(const QString& themeName) {
    if (!m_themes.contains(themeName)) {
        qWarning() << "Theme not found:" << themeName;
        return;
    }

    m_currentTheme = themeName;
    const ThemeColors& colors = m_themes[themeName];
    QPalette palette = createPaletteFromTheme(colors);

    QApplication::setPalette(palette);
}

QStringList ThemeManager::availableThemes() const {
    return m_themes.keys();
}

QString ThemeManager::currentTheme() const {
    return m_currentTheme;
}