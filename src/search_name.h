#ifndef SEARCHNAME_H
#define SEARCHNAME_H

#include <stdint.h>
#include "common.h"
#include "search_name.h"

uint32_t *cargarDirectorioName(const char *rutaDir);

OffsetList buscarEnIndiceName(const char *query, const uint32_t *dir, FILE *fidx);

#endif