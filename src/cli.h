#pragma once

#include <QStringList>

// The commands that need no overlay, for scripts and agents: list what's on
// screen, click something by name, check the setup, and fix it. Each takes
// --json, exits 0 on success and 1 on failure, and writes errors to stderr.
namespace Cli {

bool handles(const QString &command);
int run(const QStringList &arguments);
QString usage();

}
