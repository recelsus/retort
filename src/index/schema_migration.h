#pragma once

#include "sqlite_database.h"

namespace retort
{
void ensure_schema(sqlite_database &db);
}
