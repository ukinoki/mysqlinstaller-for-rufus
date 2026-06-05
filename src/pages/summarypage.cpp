#include "summarypage.h"
#include "../installwizard.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QScrollArea>

SummaryPage::SummaryPage(QWidget* parent) : QWizardPage(parent)
{
    setTitle("Récapitulatif de l'installation");
    setSubTitle("Vérifiez la configuration avant de lancer l'installation.");
    setCommitPage(true);
    setButtonText(QWizard::CommitButton, "Lancer l'installation →");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 12, 36, 12);
    root->setSpacing(10);

    m_summary = new QTextEdit();
    m_summary->setReadOnly(true);
    m_summary->setMinimumHeight(320);
    root->addWidget(m_summary);

    auto* warn = new QLabel("⚠️  Cette action va installer MySQL sur votre système. Le mot de passe administrateur vous sera demandé.");
    warn->setStyleSheet(R"(
        font-size: 12px;
        color: #854F0B;
        background: #FAEEDA;
        border: 1px solid #FAC775;
        border-radius: 8px;
        padding: 8px 12px;
    )");
    warn->setWordWrap(true);
    root->addWidget(warn);
}

void SummaryPage::initializePage() {
    auto* wiz = qobject_cast<InstallWizard*>(wizard());
    const auto& cfg = wiz->config();

    QString html = R"(
<style>
  body  { font-family: -apple-system, sans-serif; font-size: 13px; color: #1C1B18; }
  h3    { font-size: 12px; font-weight: 700; color: #534AB7; text-transform: uppercase;
          letter-spacing: 0.5px; margin: 16px 0 6px 0; border-bottom: 1px solid #EEEDFE; padding-bottom: 4px; }
  table { width: 100%; border-collapse: collapse; }
  td    { padding: 4px 8px; }
  td:first-child { color: #5F5E5A; width: 180px; }
  td:last-child  { font-weight: 500; }
  .pill { display: inline-block; background: #EEEDFE; color: #3C3489;
          border-radius: 4px; padding: 1px 6px; font-size: 11px; margin: 1px; }
  .path { font-family: 'SF Mono','Menlo',monospace; font-size: 11px; color: #5F5E5A; }
  ul    { margin: 4px 0; padding-left: 20px; }
  li    { padding: 2px 0; }
</style>
<h3>🔧 Serveur MySQL</h3>
<table>
  <tr><td>Version</td><td>MySQL )";
    html += cfg.mysqlVersion + "</td></tr>";
    html += "<tr><td>Port</td><td>" + cfg.mysqlPort + "</td></tr>";
    html += "<tr><td>Répertoire données</td><td class='path'>" + cfg.mysqlDataDir + "</td></tr>";
    html += "<tr><td>Démarrage auto</td><td>" + QString(cfg.startOnBoot ? "✅ Activé" : "❌ Désactivé") + "</td></tr>";
    html += "</table>";

    html += "<h3>👤 Utilisateur</h3><table>";
    html += "<tr><td>Nom d'utilisateur</td><td><b>" + cfg.dbUsername + "</b></td></tr>";
    html += "<tr><td>Base de données</td><td><b>" + cfg.dbName + "</b></td></tr>";
    html += "<tr><td>Mot de passe</td><td>••••••••</td></tr>";
    html += "<tr><td>Privilèges</td><td><span class='pill'>ALL PRIVILEGES</span> "
            "<span class='pill'>WITH GRANT OPTION</span></td></tr></table>";

    html += "<h3>📁 Dossiers de documents</h3><ul>";
    for (auto& f : cfg.documentFolders)
        html += "<li class='path'>" + f + "</li>";
    html += "</ul>";

    m_summary->setHtml(html);
}

int SummaryPage::nextId() const { return InstallWizard::Page_Progress; }
