#pragma once

#include <QString>
#include <QColor>
#include <QMap>
#include <QPalette>
#include <QJsonDocument>

struct ThemeColors {
    QColor window;
    QColor windowText;
    QColor base;
    QColor alternateBase;
    QColor toolTipBase;
    QColor toolTipText;
    QColor text;
    QColor button;
    QColor buttonText;
    QColor brightText;
    QColor link;
    QColor highlight;
    QColor highlightedText;
};

class ThemeManager {
public:
    static ThemeManager& instance();

    void loadThemesFromDirectory(const QString& directory);
    void applyTheme(const QString& themeName);
    QStringList availableThemes() const;
    QString currentTheme() const;

    // Convert Unreal Engine style color strings to QColor
    static QColor parseUnrealColor(const QString& colorString);

private:
    ThemeManager();
    ~ThemeManager();

    QMap<QString, ThemeColors> m_themes;
    QString m_currentTheme;

    ThemeColors parseThemeJson(const QJsonDocument& doc);
    QPalette createPaletteFromTheme(const ThemeColors& colors);
};