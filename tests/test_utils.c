#ifndef UNIT_TEST
#define UNIT_TEST 1
#endif

#include "unity/unity.h"
#include "utils.h"
#include "menu.h"
#include "camiseta.h"
#include "cancha.h"
#include "partido.h"
#include "equipo.h"
#include "lesion.h"
#include "estadisticas.h"
#include "records_rankings.h"
#include "logros.h"
#include "financiamiento.h"
#include "export_all.h"
#include "export_all_mejorado.h"
#include "import.h"
#include "torneo.h"
#include "temporada.h"
#include "entrenador_ia.h"
#include "settings.h"
#include <string.h>
#include <stdlib.h>

void setUp(void)
{
    // No setup requerido para estos tests.
}

void tearDown(void)
{
    // No cleanup requerido para estos tests.
}

static void test_safe_strnlen_basic(void)
{
    TEST_ASSERT_EQUAL_size_t(5, safe_strnlen("Hola!", 10));
    TEST_ASSERT_EQUAL_size_t(4, safe_strnlen("Hola", 4));
    TEST_ASSERT_EQUAL_size_t(0, safe_strnlen(NULL, 10));
    TEST_ASSERT_EQUAL_size_t(3, safe_strnlen("Hola", 3));
    TEST_ASSERT_EQUAL_size_t(0, safe_strnlen("Hola", 0));
}

static void test_resultado_to_text(void)
{
    TEST_ASSERT_EQUAL_STRING("VICTORIA", resultado_to_text(1));
    TEST_ASSERT_EQUAL_STRING("EMPATE", resultado_to_text(2));
    TEST_ASSERT_EQUAL_STRING("DERROTA", resultado_to_text(3));
    TEST_ASSERT_EQUAL_STRING("DESCONOCIDO", resultado_to_text(99));
}

static void test_clima_to_text(void)
{
    TEST_ASSERT_EQUAL_STRING("Despejado", clima_to_text(1));
    TEST_ASSERT_EQUAL_STRING("Nublado", clima_to_text(2));
    TEST_ASSERT_EQUAL_STRING("Lluvia", clima_to_text(3));
    TEST_ASSERT_EQUAL_STRING("Ventoso", clima_to_text(4));
    TEST_ASSERT_EQUAL_STRING("Mucho Calor", clima_to_text(5));
    TEST_ASSERT_EQUAL_STRING("Mucho Frio", clima_to_text(6));
    TEST_ASSERT_EQUAL_STRING("DESCONOCIDO", clima_to_text(0));
}

static void test_dia_to_text(void)
{
    TEST_ASSERT_EQUAL_STRING("Dia", dia_to_text(1));
    TEST_ASSERT_EQUAL_STRING("Tarde", dia_to_text(2));
    TEST_ASSERT_EQUAL_STRING("Noche", dia_to_text(3));
    TEST_ASSERT_EQUAL_STRING("DESCONOCIDO", dia_to_text(4));
}

static void test_trim_trailing_spaces(void)
{
    char buffer1[32] = "Hola   ";
    char buffer2[32] = "Hola\t\n\r";
    char buffer3[32] = "Hola";
    char buffer4[32] = "Hola   \t\n";

    TEST_ASSERT_EQUAL_STRING("Hola", trim_trailing_spaces(buffer1));
    TEST_ASSERT_EQUAL_STRING("Hola", trim_trailing_spaces(buffer2));
    TEST_ASSERT_EQUAL_STRING("Hola", trim_trailing_spaces(buffer3));
    TEST_ASSERT_EQUAL_STRING("Hola", trim_trailing_spaces(buffer4));
}

static void test_format_date_for_display(void)
{
    char output[32];
    format_date_for_display("2026-02-01 10:30", output, sizeof(output));
    TEST_ASSERT_EQUAL_STRING("01/02/2026 10:30", output);
}

static void test_convert_display_date_to_storage(void)
{
    char output[32];
    convert_display_date_to_storage("31/12/2025 23:59", output, sizeof(output));
    TEST_ASSERT_EQUAL_STRING("2025-12-31 23:59", output);
}

static void test_remover_tildes(void)
{
    const char input1[] = "Camiseta \xE1\xE9\xED\xF3\xFA";
    const char input2[] = "Ni\xF1o y ping\xFCino";

    TEST_ASSERT_EQUAL_STRING("Camiseta aeiou", remover_tildes(input1));
    TEST_ASSERT_EQUAL_STRING("Nino y pinguino", remover_tildes(input2));
}

static void test_menu_items_metadata(void)
{
    int count = menu_get_item_count();
    const MenuItem *items = menu_get_items();

    TEST_ASSERT_TRUE(count > 0);
    TEST_ASSERT_NOT_NULL(items);
    TEST_ASSERT_EQUAL_INT(0, items[count - 1].opcion);
    TEST_ASSERT_EQUAL_STRING("Salir", items[count - 1].texto);
    TEST_ASSERT_TRUE(items[count - 1].accion == NULL);

    TEST_ASSERT_EQUAL_INT(1, items[0].opcion);
    TEST_ASSERT_EQUAL_STRING("Camisetas", items[0].texto);
    TEST_ASSERT_TRUE(items[0].accion != NULL);
}

static void test_menu_buscar_item(void)
{
    int count = menu_get_item_count();
    const MenuItem *items = menu_get_items();

    const MenuItem *found = menu_buscar_item(items, count, 1);
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("Camisetas", found->texto);

    const MenuItem *not_found = menu_buscar_item(items, count, 999);
    TEST_ASSERT_TRUE(not_found == NULL);
}

static void test_create_filtered_menu(void)
{
    int count = 0;
    const MenuItem *items = menu_get_items();
    int base_count = menu_get_item_count();

    MenuItem *filtered = create_filtered_menu(&count);
    TEST_ASSERT_NOT_NULL(filtered);
    TEST_ASSERT_EQUAL_INT(base_count, count);

    for (int i = 0; i < count - 1; i++)
    {
        TEST_ASSERT_EQUAL_INT(i + 1, filtered[i].opcion);
        TEST_ASSERT_EQUAL_STRING(items[i].texto, filtered[i].texto);
        TEST_ASSERT_TRUE(filtered[i].accion != NULL);
    }

    TEST_ASSERT_EQUAL_INT(0, filtered[count - 1].opcion);
    TEST_ASSERT_EQUAL_STRING("Salir", filtered[count - 1].texto);
    TEST_ASSERT_TRUE(filtered[count - 1].accion == NULL);

    free(filtered);
}

typedef void (*MenuFunc)(void);

static void assert_menu_exec(MenuFunc func)
{
    MenuTestCapture capture = {0};
    menu_test_set_capture(&capture);

    func();

    menu_test_set_capture(NULL);
    TEST_ASSERT_NOT_NULL(capture.titulo);
    TEST_ASSERT_TRUE(capture.cantidad > 0);
    TEST_ASSERT_TRUE(capture.last_item.accion == NULL);
}

static void test_menu_functions_smoke(void)
{
    assert_menu_exec(&menu_camisetas);
    assert_menu_exec(&menu_canchas);
    assert_menu_exec(&menu_partidos);
    assert_menu_exec(&menu_equipos);
    assert_menu_exec(&menu_lesiones);
    assert_menu_exec(&menu_estadisticas);
    assert_menu_exec(&menu_estadisticas_generales);
    assert_menu_exec(&menu_estadisticas_partidos);
    assert_menu_exec(&menu_estadisticas_goles);
    assert_menu_exec(&menu_estadisticas_asistencias);
    assert_menu_exec(&menu_estadisticas_rendimiento);

    assert_menu_exec(&menu_records_rankings);
    assert_menu_exec(&menu_logros);
    assert_menu_exec(&menu_financiamiento);
    assert_menu_exec(&menu_presupuestos_mensuales);
    assert_menu_exec(&menu_exportar);
    assert_menu_exec(&menu_exportar_mejorado);
    assert_menu_exec(&menu_importar);
    assert_menu_exec(&menu_torneos);
    assert_menu_exec(&menu_temporadas);
    assert_menu_exec(&menu_entrenador_ia);
    assert_menu_exec(&menu_settings);
    assert_menu_exec(&menu_usuario);
}

static void test_menu_custom_menus_smoke(void)
{
    menu_custom_menus();
    TEST_ASSERT_TRUE(1);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_safe_strnlen_basic);
    RUN_TEST(test_resultado_to_text);
    RUN_TEST(test_clima_to_text);
    RUN_TEST(test_dia_to_text);
    RUN_TEST(test_trim_trailing_spaces);
    RUN_TEST(test_format_date_for_display);
    RUN_TEST(test_convert_display_date_to_storage);
    RUN_TEST(test_remover_tildes);
    RUN_TEST(test_menu_items_metadata);
    RUN_TEST(test_menu_buscar_item);
    RUN_TEST(test_create_filtered_menu);
    RUN_TEST(test_menu_functions_smoke);
    RUN_TEST(test_menu_custom_menus_smoke);

    return UNITY_END();
}
