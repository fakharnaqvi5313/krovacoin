// Copyright (c) 2019-2022 The KROVACOIN Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/krovacoin-config.h"
#endif

#include "qt/krovacoin/settings/settingsfaqwidget.h"
#include "qt/krovacoin/settings/forms/ui_settingsfaqwidget.h"
#include "qt/krovacoin/qtutils.h"

#include <QScrollBar>
#include <QMetaObject>

SettingsFaqWidget::SettingsFaqWidget(KROVACOINGUI* parent) :
    QDialog(parent),
    ui(new Ui::SettingsFaqWidget)
{
    ui->setupUi(this);
    this->setStyleSheet(parent->styleSheet());

#ifdef Q_OS_MAC
    ui->container->load("://bg-welcome");
    setCssProperty(ui->container, "container-welcome-no-image");
#else
    setCssProperty(ui->container, "container-welcome");
#endif
    setCssProperty(ui->labelTitle, "text-title-faq");
    setCssProperty(ui->labelWebLink, "text-content-white");

    // Content
    setCssProperty({
           ui->labelNumber_Intro,
           ui->labelNumber_UnspendablePIV,
           ui->labelNumber_Stake,
           ui->labelNumber_Support,
           ui->labelNumber_Masternode,
           ui->labelNumber_MNController
        }, "container-number-faq");

    setCssProperty({
              ui->labelSubtitle_Intro,
              ui->labelSubtitle_UnspendablePIV,
              ui->labelSubtitle_Stake,
              ui->labelSubtitle_Support,
              ui->labelSubtitle_Masternode,
              ui->labelSubtitle_MNController
            }, "text-subtitle-faq");


    setCssProperty({
              ui->labelContent_Intro,
              ui->labelContent_UnspendablePIV,
              ui->labelContent_Stake,
              ui->labelContent_Support,
              ui->labelContent_Masternode,
              ui->labelContent_MNController
            }, "text-content-faq");


    setCssProperty({
              ui->pushButton_Intro,
              ui->pushButton_UnspendablePIV,
              ui->pushButton_Stake,
              ui->pushButton_Support,
              ui->pushButton_Masternode,
              ui->pushButton_MNController
            }, "btn-faq-options");

    ui->labelContent_Support->setOpenExternalLinks(true);

    // Set FAQ content strings
    QString introContent = formatFAQContent(
        formatFAQParagraph(
            tr("KROVACOIN is a form of digital online money using blockchain technology "
               "that can be easily transferred globally, instantly, and with near "
               "zero fees. KROVACOIN incorporates market leading security & "
               "privacy and is also the first PoS (Proof of Stake) Cryptocurrency "
               "to implement Sapling(SHIELD), a zk-SNARKs based privacy protocol.")) +
        formatFAQParagraph(
            tr("KROVACOIN utilizes a Proof of Stake (PoS) consensus system algorithm, "
               "allowing all owners of KROVACOIN to participate in securing the network "
               "and earning rewards by staking with a full node wallet.")));
    ui->labelContent_Intro->setText(introContent);

    QString unspendablePIVContent = formatFAQContent(
        formatFAQParagraph(
            tr("Newly received KROVACOIN requires 6 confirmations on the network "
               "to become eligible for spending which can take ~6 minutes.")) +
        formatFAQParagraph(
            tr("Your KROVACOIN wallet also needs to be completely synchronized "
               "to see and spend balances on the network.")));
    ui->labelContent_UnspendablePIV->setText(unspendablePIVContent);

    QString stakeContent = formatFAQContent(
        formatFAQOrderedList(
            formatFAQListItem(tr("Make sure your wallet is completely synchronized and you are using the latest release.")) +
            formatFAQListItem(tr("You must have a balance of KROVACOIN with a minimum of 600 confirmations.")) +
            formatFAQListItem(tr("Your wallet must stay online and be unlocked for staking purposes.")) +
            formatFAQListItem(tr("Once all those steps are followed staking should be enabled."))) +
        formatFAQParagraph(
            tr("You can see the status of staking in the wallet by mousing over the "
               "package icon in the row on the top left of the wallet interface. The "
               "package will be lit up and will state \"Staking Enabled\" to indicate "
               "it is staking. Using the command line interface (%1); the command %2 "
               "will confirm that staking is active.")
                .arg("krova-cli", "<span style=\"font-style:italic\">getstakingstatus</span>")));
    ui->labelContent_Stake->setText(stakeContent);

    QString supportContent = formatFAQContent(
        formatFAQParagraph(
            tr("We have support channels in most of our official chat groups, for example %1")
                .arg("<a style='color: #b088ff' href='https://discord.KROVACOIN.org'>" + tr("#support in our Discord") + "</a>.")));
    ui->labelContent_Support->setText(supportContent);

    QString masternodeContent = formatFAQContent(
        formatFAQParagraph(
            tr("%1 does not use masternodes. The network is secured entirely by regular "
               "Proof of Stake, including native cold staking, which lets you delegate "
               "the right to stake your coins to a hot node without ever exposing your "
               "spending keys.")
                .arg(PACKAGE_NAME)));
    ui->labelContent_Masternode->setText(masternodeContent);

    QString mNControllerContent = formatFAQContent(
        formatFAQParagraph(
            tr("Not applicable -- %1 has no masternode or controller-wallet concept. "
               "See the Staking FAQ entry for how to delegate stake instead.")
                    .arg(PACKAGE_NAME)));
    ui->labelContent_MNController->setText(mNControllerContent);


    // Exit button
    setCssProperty(ui->pushButtonExit, "btn-faq-exit");

    // Web Link
    setCssProperty(ui->pushButtonWebLink, "btn-faq-web");
    setCssProperty(ui->containerButtons, "container-faq-buttons");

    // Buttons
    connect(ui->pushButtonExit, &QPushButton::clicked, this, &SettingsFaqWidget::close);
    connect(ui->pushButton_Intro, &QPushButton::clicked, [this](){onFaqClicked(ui->widget_Intro);});
    connect(ui->pushButton_UnspendablePIV, &QPushButton::clicked, [this](){onFaqClicked(ui->widget_UnspendablePIV);});
    connect(ui->pushButton_Stake, &QPushButton::clicked, [this](){onFaqClicked(ui->widget_Stake);});
    connect(ui->pushButton_Support, &QPushButton::clicked, [this](){onFaqClicked(ui->widget_Support);});
    connect(ui->pushButton_Masternode, &QPushButton::clicked, [this](){onFaqClicked(ui->widget_Masternode);});
    connect(ui->pushButton_MNController, &QPushButton::clicked, [this](){onFaqClicked(ui->widget_MNController);});

    if (parent)
        connect(parent, &KROVACOINGUI::windowResizeEvent, this, &SettingsFaqWidget::windowResizeEvent);
}

void SettingsFaqWidget::showEvent(QShowEvent *event)
{
    QPushButton* btn = getButtons()[section];
    QMetaObject::invokeMethod(btn, "setChecked", Qt::QueuedConnection, Q_ARG(bool, true));
    QMetaObject::invokeMethod(btn, "clicked", Qt::QueuedConnection);
}

void SettingsFaqWidget::setSection(Section _section)
{
    section = _section;
}

void SettingsFaqWidget::onFaqClicked(const QWidget* const widget)
{
    ui->scrollAreaFaq->verticalScrollBar()->setValue(widget->y());
}

void SettingsFaqWidget::windowResizeEvent(QResizeEvent* event)
{
    QWidget* w = qobject_cast<QWidget*>(parent());
    this->resize(w->width(), w->height());
    this->move(QPoint(0, 0));
}

std::vector<QPushButton*> SettingsFaqWidget::getButtons()
{
    return {
            ui->pushButton_Intro,
            ui->pushButton_UnspendablePIV,
            ui->pushButton_Stake,
            ui->pushButton_Support,
            ui->pushButton_Masternode,
            ui->pushButton_MNController
    };
}

SettingsFaqWidget::~SettingsFaqWidget()
{
    delete ui;
}


