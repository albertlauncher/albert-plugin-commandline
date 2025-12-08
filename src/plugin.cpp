// Copyright (c) 2017-2025 Manuel Schneider

#include "plugin.h"
#include <QDirIterator>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QStringList>
#include <albert/logging.h>
#include <albert/extensionregistry.h>
#include <albert/iconutil.h>
#include <albert/standarditem.h>
#include <albert/systemutil.h>
ALBERT_LOGGING_CATEGORY("commandline")
using namespace Qt::StringLiterals;
using namespace albert;
using namespace std;

namespace{
static auto getPaths() { return qEnvironmentVariable("PATH").split(u':', Qt::SkipEmptyParts); }
static unique_ptr<Icon> makeIcon() { return makeImageIcon(u":commandline.svg"_s); }
}

Plugin::Plugin()
{
    indexer_.parallel = [](const bool &abort)
    {
        set<QString> executables;
        const auto paths = getPaths();
        DEBG << "Indexing" << paths.join(u", "_s);

        for (const QString &path : paths)
        {
            QDirIterator dirIt(path,
                               QDir::NoDotAndDotDot|QDir::Files|QDir::Executable,
                               QDirIterator::Subdirectories);

            while (dirIt.hasNext())
            {
                if (abort)
                    return executables;
                dirIt.next();
                executables.insert(dirIt.fileName());
            }
        }

        return executables;
    };

    indexer_.finish = [this]
    {
        index_ = indexer_.takeResult();
        INFO << u"Indexed %1 executables"_s.arg(index_.size());
    };

    watcher_.addPaths(getPaths());
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this](){ indexer_.run(); });

    indexer_.run();
}

Plugin::~Plugin() = default;

QWidget *Plugin::buildConfigWidget()
{
    auto t = uR"(<ul style="margin-left:-1em">)"_s;
    for (const auto paths = getPaths();
         const auto &path : paths)
        t += uR"(<li><a href="file://%1")>%1</a></li>)"_s.arg(path);
    t += u"</ul>"_s;

    auto l = new QLabel(t);
    l->setOpenExternalLinks(true);
    l->setWordWrap(true);
    l->setAlignment(Qt::AlignTop);
    return l;
}

QString Plugin::synopsis(const QString &) const { return tr("<command> [params]"); }

QString Plugin::defaultTrigger() const { return u">"_s; }

vector<Action> Plugin::buildActions(const QString &commandline) const
{
    vector<Action> a;

    a.emplace_back(u"r"_s, tr("Run in terminal"),
                   [=, this]{ apps_->runTerminal(u"%1 ; exec $SHELL"_s.arg(commandline)); });

    a.emplace_back(u"rc"_s, tr("Run in terminal and close on exit"),
                   [=, this]{ apps_->runTerminal(commandline); });

    a.emplace_back(u"rb"_s, tr("Run in background (without terminal)"),
                   [=]{ runDetachedProcess({u"sh"_s, u"-c"_s, commandline}); });

    return a;
}

vector<RankItem> Plugin::handleGlobalQuery(Query &query)
{
    vector<RankItem> matches;

    if (query.string().trimmed().isEmpty())
        return matches;

    // Extract data from input string: [0] program. The rest: args
    QString potentialProgram = query.string().section(u' ', 0, 0, QString::SectionSkipEmpty);
    QString remainder = query.string().section(u' ', 1, -1, QString::SectionSkipEmpty);

    static const auto tr_rcmd = tr("Run '%1'");

    QString commonPrefix;
    if (auto it = lower_bound(index_.begin(), index_.end(), potentialProgram);
        it != index_.end())
    {
        commonPrefix = *it;

        while (it != index_.end() && it->startsWith(potentialProgram)) {

            // Update common prefix
            auto mismatchindexes = mismatch(it->begin() + potentialProgram.size() - 1, it->end(),
                                            commonPrefix.begin() + potentialProgram.size() - 1);
            commonPrefix.resize(distance(it->begin(), mismatchindexes.first));

            auto commandline = remainder.isEmpty() ? *it : u"%1 %2"_s.arg(*it, remainder);

            matches.emplace_back(StandardItem::make(*it,
                                                    commandline,
                                                    tr_rcmd.arg(commandline),
                                                    makeIcon,
                                                    buildActions(commandline),
                                                    commonPrefix),
                                 double(query.string().length()) / it->length());
            ++it;
        }

        // Apply completion string to items
        QString completion = u"%1 %2"_s.arg(commonPrefix, remainder);
        for (auto &item : matches)
            static_pointer_cast<StandardItem>(item.item)->setInputActionText(completion);
    }

    // Build feeling lucky item in triggered mode
    if (!query.trigger().isEmpty())
    {

        static const auto tr_title = tr("I'm Feeling Lucky");
        static const auto tr_description = tr("Try running '%1'");
        matches.emplace_back(StandardItem::make({},
                                                tr_title,
                                                tr_description.arg(query.string()),
                                                makeIcon,
                                                buildActions(query.string())),
                             .0);
    }

    return matches;
}
