// Copyright (c) 2017-2024 Manuel Schneider

#pragma once
#include <QFileSystemWatcher>
#include <albert/backgroundexecutor.h>
#include <albert/extensionplugin.h>
#include <albert/plugin/applications.h>
#include <albert/plugindependency.h>
#include <albert/globalqueryhandler.h>
#include <set>
namespace albert { class Action; }

class Plugin : public albert::ExtensionPlugin,
               public albert::GlobalQueryHandler
{
    ALBERT_PLUGIN

public:

    Plugin();
    ~Plugin();

    QWidget *buildConfigWidget() override;
    QString synopsis(const QString &) const override;
    QString defaultTrigger() const override;
    std::vector<albert::RankItem> rankItems(albert::QueryContext &) override;

private:

    std::vector<albert::Action> buildActions(const QString &commandline) const;

    QFileSystemWatcher watcher_;
    std::set<QString> index_;
    albert::BackgroundExecutor<std::set<QString>> indexer_;
    albert::StrongDependency<applications::Plugin> apps_{QStringLiteral("applications")};

};
