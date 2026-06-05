#pragma once
#include <QWizardPage>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QTreeWidget>

class FoldersPage : public QWizardPage {
    Q_OBJECT
public:
    explicit FoldersPage(QWidget* parent = nullptr);
    int  nextId()       const override;
    bool validatePage()       override;
    void initializePage()     override;

private:
    QTreeWidget* m_tree;
    QLineEdit*   m_newFolder;
    QString      m_basePath;

    void addFolderToTree(const QString& path, QTreeWidgetItem* parent = nullptr);
    void refreshTree();
    QStringList collectPaths(QTreeWidgetItem* item, const QString& base);
};
