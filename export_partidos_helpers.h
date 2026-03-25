#ifndef EXPORT_PARTIDOS_HELPERS_H
#define EXPORT_PARTIDOS_HELPERS_H

#include "db.h"
#include "utils.h"
#include "cJSON.h"
#include "sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===================== DATABASE HELPERS ===================== */

/**
 * Checks if there are any partido records in the database.
 * Returns 1 if records exist, 0 if no records found.
 */
static int check_partido_records()
{
    sqlite3_stmt *check_stmt;
    int count = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM partido", -1, &check_stmt, NULL) != SQLITE_OK)
    {
        return 0;
    }
    if (sqlite3_step(check_stmt) == SQLITE_ROW)
    {
        count = sqlite3_column_int(check_stmt, 0);
    }
    sqlite3_finalize(check_stmt);
    return count > 0;
}

/**
 * Prepares a partido query with the given order by clause.
 * Centralizes the common SQL query used by most export functions.
 */
static sqlite3_stmt* prepare_partido_query(const char* order_by_clause)
{
    sqlite3_stmt *stmt;
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT can.nombre,p.fecha_hora,p.goles,p.asistencias,c.nombre,p.resultado,p.clima,p.dia,p.rendimiento_general,p.cansancio,p.estado_animo,p.comentario_personal "
             "FROM partido p JOIN camiseta c ON p.camiseta_id=c.id "
             "JOIN cancha can ON p.cancha_id = can.id %s",
             order_by_clause ? order_by_clause : "");
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK)
    {
        return NULL;
    }
    return stmt;
}

/* ===================== FILE HELPERS ===================== */

/**
 * Opens an export file with the given filename.
 * Handles error checking and returns the file pointer.
 */
static FILE* open_export_file(const char* filename)
{
    FILE *f = NULL;
    fopen_s(&f, get_export_path(filename), "w");
    return f;
}

/**
 * Closes an export file and handles error checking.
 */
static void close_export_file(FILE* file)
{
    if (file)
    {
        fclose(file);
    }
}

/* ===================== DATA PROCESSING HELPERS ===================== */

// trim_cancha_text removida - usar trim_trailing_spaces de utils.h directamente

/* ===================== FORMAT-SPECIFIC HELPERS ===================== */

/* ===================== GENERIC EXPORT HELPERS ===================== */

/**
 * Generic export function for handling common export patterns.
 * Takes a filename and a write function pointer to handle format-specific writing.
 */
static void export_partidos_generic(const char* filename, void (*write_function)(FILE*, sqlite3_stmt*))
{
    if (!check_partido_records())
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = open_export_file(filename);
    if (!f)
        return;

    sqlite3_stmt *stmt = prepare_partido_query(NULL);
    if (!stmt)
    {
        close_export_file(f);
        return;
    }
    write_function(f, stmt);
    sqlite3_finalize(stmt);

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    close_export_file(f);
}

/**
 * Generic export function for handling specific partido exports.
 * Takes an order by clause, filename, and write function pointer.
 */
static void export_partido_especifico_generic(const char* order_by_clause, const char* filename, void (*write_function)(FILE*, sqlite3_stmt*))
{
    if (!check_partido_records())
    {
        mostrar_no_hay_registros("partidos para exportar");
        return;
    }

    FILE *f = open_export_file(filename);
    if (!f)
        return;

    sqlite3_stmt *stmt = prepare_partido_query(order_by_clause);
    if (!stmt)
    {
        close_export_file(f);
        return;
    }
    write_function(f, stmt);
    sqlite3_finalize(stmt);

    printf("Archivo exportado a: %s\n", get_export_path(filename));
    close_export_file(f);
}

#endif // EXPORT_PARTIDOS_HELPERS_H
