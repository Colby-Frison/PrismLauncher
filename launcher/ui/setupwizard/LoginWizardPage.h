#pragma once
#include <QWidget>
#include "BaseWizardPage.h"

namespace Ui {
class LoginWizardPage;
}

class AccountCoordinator;

class LoginWizardPage : public BaseWizardPage {
    Q_OBJECT

   public:
    explicit LoginWizardPage(AccountCoordinator* accountCoordinator, QWidget* parent = nullptr);
    ~LoginWizardPage();

    void initializePage() override;
    bool validatePage() override;
    void retranslate() override;
   private slots:
    void on_pushButton_clicked();

   private:
    AccountCoordinator* m_accountCoordinator = nullptr;
    Ui::LoginWizardPage* ui;
};
