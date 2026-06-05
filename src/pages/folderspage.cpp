#include "folderspage.h"
#include "../installwizard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QStandardPaths>

FoldersPage::FoldersPage(QWidget* parent) : QWizardPage(parent)
{
    setTitle("Dossiers de documents");
    setSubTitle("Définissez la structure de répertoires à créer pour vos documents.");

    m_basePath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                 + "/MySQLDocs";

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 16, 36, 16);
    root->setSpacing(12);

    // Chemin de base
    auto* baseGroup  = new QGroupBox("Dossier racine");
    auto* baseLayout = new QHBoxLayout(baseGroup);
    baseLayout->setContentsMargins(16, 12, 16, 12);

    auto* baseLbl = new QLabel(m_basePath);
    baseLbl->setStyleSheet("font-size: 13px; font-family: 'SF Mono','Menlo',monospace; color: #5F5E5A;");
    baseLbl->setWordWrap(true);

    auto* changeBase = new QPushButton("Changer…");
    changeBase->setFixedWidth(90);
    connect(changeBase, &QPushButton::clicked, this, [this, baseLbl]{
        QString dir = QFileDialog::getExistingDirectory(this,
            "Choisir le dossier racine",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
        if (!dir.isEmpty()) {
            m_basePath = dir + "/MySQLDocs";
            baseLbl->setText(m_basePath);
            refreshTree();
        }
    });

    baseLayout->addWidget(baseLbl, 1);
    baseLayout->addWidget(changeBase);
    root->addWidget(baseGroup);

    // Arbre
    auto* treeGroup  = new QGroupBox("Structure des dossiers");
    auto* treeLayout = new QVBoxLayout(treeGroup);
    treeLayout->setContentsMargins(12, 10, 12, 12);
    treeLayout->setSpacing(8);

    m_tree = new QTreeWidget();
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(20);
    m_tree->setAnimated(true);
    m_tree->setMinimumHeight(180);
    m_tree->setStyleSheet(R"(
        QTreeWidget {
            background: #FFFFFF;
            border: 1px solid #D3D1C7;
            border-radius: 10px;
        }
        QTreeWidget::item {
            padding: 5px 8px;
            border-radius: 6px;
        }
        QTreeWidget::item:selected {
            background: #EEEDFE;
            color: #3C3489;
        }
        QTreeWidget::branch {
            background: transparent;
        }
    )");

    // Boutons d'action
    auto* btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_newFolder = new QLineEdit();
    m_newFolder->setPlaceholderText("Nom du nouveau dossier…");

    auto* addBtn    = new QPushButton("+ Ajouter");
    auto* addSubBtn = new QPushButton("+ Sous-dossier");
    auto* removeBtn = new QPushButton("Supprimer");
    addBtn->setFixedWidth(100);
    addSubBtn->setFixedWidth(120);
    removeBtn->setFixedWidth(100);
    removeBtn->setStyleSheet(R"(
        QPushButton {
            color: #A32D2D;
            border-color: #F7C1C1;
            background: #FCEBEB;
        }
        QPushButton:hover { background: #F7C1C1; }
    )");

    connect(addBtn, &QPushButton::clicked, this, [this]{
        QString name = m_newFolder->text().trimmed();
        if (name.isEmpty()) return;
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, "📁  " + name);
        item->setData(0, Qt::UserRole, name);
        m_tree->expandAll();
        m_newFolder->clear();
    });

    connect(addSubBtn, &QPushButton::clicked, this, [this]{
        auto* sel = m_tree->currentItem();
        if (!sel) { return; }
        QString name = m_newFolder->text().trimmed();
        if (name.isEmpty()) return;
        auto* child = new QTreeWidgetItem(sel);
        child->setText(0, "📁  " + name);
        child->setData(0, Qt::UserRole, name);
        sel->setExpanded(true);
        m_newFolder->clear();
    });

    connect(removeBtn, &QPushButton::clicked, this, [this]{
        auto* sel = m_tree->currentItem();
        if (!sel) return;
        delete sel;
    });

    btnRow->addWidget(m_newFolder, 1);
    btnRow->addWidget(addBtn);
    btnRow->addWidget(addSubBtn);
    btnRow->addWidget(removeBtn);

    treeLayout->addWidget(m_tree);
    treeLayout->addLayout(btnRow);
    root->addWidget(treeGroup);

    // Note
    auto* note = new QLabel("💡 Les dossiers seront créés sous le chemin racine ci-dessus.");
    note->setStyleSheet("font-size: 12px; color: #888780;");
    root->addWidget(note);
    root->addStretch();
}

void FoldersPage::initializePage() {
    refreshTree();
}

void FoldersPage::refreshTree() {
    m_tree->clear();

    // Dossiers par défaut
    QStringList defaults = {"Backups", "Exports", "Imports", "Scripts", "Rapports"};
    for (auto& d : defaults) {
        auto* item = new QTreeWidgetItem(m_tree);
        item->setText(0, "📁  " + d);
        item->setData(0, Qt::UserRole, d);
    }
    // Sous-dossiers par défaut
    auto* exports = m_tree->topLevelItem(1);
    if (exports) {
        auto* csv  = new QTreeWidgetItem(exports);
        csv->setText(0, "📁  CSV");
        csv->setData(0, Qt::UserRole, "CSV");
        auto* json = new QTreeWidgetItem(exports);
        json->setText(0, "📁  JSON");
        json->setData(0, Qt::UserRole, "JSON");
        exports->setExpanded(true);
    }
    m_tree->expandAll();
}

QStringList FoldersPage::collectPaths(QTreeWidgetItem* item, const QString& base) {
    QString current = base + "/" + item->data(0, Qt::UserRole).toString();
    QStringList paths;
    paths << current;
    for (int i = 0; i < item->childCount(); ++i)
        paths << collectPaths(item->child(i), current);
    return paths;
}

bool FoldersPage::validatePage() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    QStringList all;
    all << m_basePath;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        all << collectPaths(m_tree->topLevelItem(i), m_basePath);
    wiz->config().documentFolders = all;
    return true;
}

int FoldersPage::nextId() const { return InstallWizard::Page_Summary; }
