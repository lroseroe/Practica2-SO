#ifndef SEARCHPUBLISHER_H
#define SEARCHPUBLISHER_H

#include <stdint.h>
#include "common.h"
#include "search_publisher.h"

uint32_t *cargarDirectorioPublisher(const char *rutaDir);

OffsetList buscarEnIndicePublisher(const char *query, const uint32_t *dir, FILE *fidx);

#endif