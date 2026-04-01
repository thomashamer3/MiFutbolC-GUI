#include "menu.h"
#include "utils.h"
#include "gui.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <errno.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


static int g_lock_fd = -1;
static char g_lock_path[1024];

static void release_single_instance_lock(void)
{
    if (g_lock_fd >= 0)
    {
        flock(g_lock_fd, LOCK_UN);
        close(g_lock_fd);
        g_lock_fd = -1;
    }

    if (g_lock_path[0] != '\0')
    {
        unlink(g_lock_path);
        g_lock_path[0] = '\0';
    }
}

static int acquire_single_instance_lock(void)
{
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir || runtime_dir[0] == '\0')
    {
        runtime_dir = "/tmp";
    }

    snprintf(g_lock_path, sizeof(g_lock_path), "%s/%s", runtime_dir,
             "mifutbolc.lock");

    g_lock_fd = open(g_lock_path, O_CREAT | O_RDWR, 0644);
    if (g_lock_fd < 0)
    {
        fprintf(stderr, "No se pudo crear lockfile (%s): %s\n", g_lock_path,
                strerror(errno));
        return 0;
    }

    if (flock(g_lock_fd, LOCK_EX | LOCK_NB) != 0)
    {
        if (errno == EWOULDBLOCK)
        {
            fprintf(stderr,
                    "MiFutbolC ya esta en ejecucion. Cierra la otra instancia e intenta nuevamente.\n");
        }
        else
        {
            fprintf(stderr, "No se pudo bloquear lockfile (%s): %s\n",
                    g_lock_path, strerror(errno));
        }

        close(g_lock_fd);
        g_lock_fd = -1;
        g_lock_path[0] = '\0';
        return 0;
    }

    atexit(release_single_instance_lock);
    return 1;
}
#endif

static int run_gui_flow(MenuItem *filtered_items, int count)
{
    gui_set_context_title("Menu Principal");

    while (1)
    {
        int gui_result = run_raylib_gui(filtered_items, count);
        int selected_index = gui_get_last_selected_index();

        if (gui_result == GUI_ACTION_OPEN_CLASSIC_MENU)
        {
            continue;
        }

        if (gui_result == GUI_ACTION_RUN_SELECTED_OPTION)
        {
            if (selected_index < 0 || selected_index >= count)
                continue;

            if (!filtered_items[selected_index].accion)
            {
                free(filtered_items);
                db_close();
                return 0;
            }

            filtered_items[selected_index].accion();
            continue;
        }

        free(filtered_items);
        db_close();
        return 0;
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

#ifndef _WIN32
    if (!acquire_single_instance_lock())
    {
        return 1;
    }
#endif

    if (!iniciar_sesion_multiusuario_local())
    {
        return 0;
    }

    initialize_application();
    handle_user_name();

    int count;
    MenuItem* filtered_items = create_filtered_menu(&count);

    menu_set_gui_enabled(1);
    return run_gui_flow(filtered_items, count);
}
