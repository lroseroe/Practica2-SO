#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <gtk/gtk.h> //GUI
#include <unistd.h> //Para read/write
#include <fcntl.h> //Para open
#include "common.h"

//Tuberías con nombre
int fd_query, fd_response;

typedef struct {
    GtkWidget *window;
    GtkWidget *name_entry;
    GtkWidget *publ_entry;
    GtkWidget *result_list; //Para mostrar resultados en una lista

    Videojuego *results;
    int result_count;

    GtkWidget *name_lbl;
    GtkWidget *release_date_lbl;
    GtkWidget *background_image_lbl;
    GtkWidget *rating_lbl;
    GtkWidget *ratings_count_lbl;
    GtkWidget *added_lbl;
    GtkWidget *playtime_lbl;
    GtkWidget *reviews_count_lbl;
    GtkWidget *platforms_lbl;
    GtkWidget *stores_lbl;
    GtkWidget *developers_lbl;
    GtkWidget *genres_lbl;
    GtkWidget *publishers_lbl;
    GtkWidget *website_lbl;
    GtkWidget *description_lbl;
} searchWidgets;

char *stringArrayToString(char arr[][MAX_STRING_LEN], int n){
    GString *result = g_string_new("");

    for(int i = 0; i < n; i++){
        if(arr[i][0] == '\0')
            break;

        g_string_append(result, arr[i]);

        if(i != n - 1 && arr[i + 1][0] != '\0'){
            g_string_append(result, ", ");
        }
    }

    return g_string_free(result, FALSE); 
}

static void showError(GtkWindow *parent, const char *message){
    GtkAlertDialog *dialog;
    dialog = gtk_alert_dialog_new("%s", message);
    gtk_alert_dialog_show(dialog, parent);
}

static void mostrarDetalles(GtkListBox *box, GtkListBoxRow *row, gpointer user_data){
    if(!row) return;
    searchWidgets *widgets = user_data;
    char *text; //String auxiliar para mostrar info en pantalla
    char *temp; //Para manejar stringArrayToString
    Videojuego *vjuego = g_object_get_data(G_OBJECT(row), "game-data");
    if(!vjuego) return;

    //g_strdup_printf() aloca nueva memoria para el string deseado
    text = g_strdup_printf("Nombre: %s", vjuego->name);
    gtk_label_set_text(GTK_LABEL(widgets->name_lbl), text);
    g_free(text);

    text = g_strdup_printf("Fecha de lanzamiento: %s", vjuego->release_date);
    gtk_label_set_text(GTK_LABEL(widgets->release_date_lbl), text);
    g_free(text);

    text = g_strdup_printf("Imagen de fondo: %s", vjuego->background_image);
    gtk_label_set_text(GTK_LABEL(widgets->background_image_lbl), text);
    g_free(text);

    text = g_strdup_printf("Calificación: %.2f", vjuego->rating);
    gtk_label_set_text(GTK_LABEL(widgets->rating_lbl), text);
    g_free(text);

    text = g_strdup_printf("Cantidad de calificaciones: %i", vjuego->ratings_count);
    gtk_label_set_text(GTK_LABEL(widgets->ratings_count_lbl), text);
    g_free(text);

    text = g_strdup_printf("Cantidad de jugadores: %i", vjuego->added);
    gtk_label_set_text(GTK_LABEL(widgets->added_lbl), text);
    g_free(text);

    text = g_strdup_printf("Tiempo jugado (Horas): %i", vjuego->playtime);
    gtk_label_set_text(GTK_LABEL(widgets->playtime_lbl), text);
    g_free(text);

    text = g_strdup_printf("Cantidad de reseñas: %i", vjuego->reviews_count);
    gtk_label_set_text(GTK_LABEL(widgets->reviews_count_lbl), text);
    g_free(text);

    temp = stringArrayToString(vjuego->platforms, MAX_CANT);
    text = g_strdup_printf("Plataformas: %s", temp);
    gtk_label_set_text(GTK_LABEL(widgets->platforms_lbl), text);
    g_free(temp);
    g_free(text);

    temp = stringArrayToString(vjuego->stores, MAX_CANT);
    text = g_strdup_printf("Tiendas: %s", temp);
    gtk_label_set_text(GTK_LABEL(widgets->stores_lbl), text);
    g_free(temp);
    g_free(text);

    temp = stringArrayToString(vjuego->developers, MAX_CANT);
    text = g_strdup_printf("Desarrolladores: %s", temp);
    gtk_label_set_text(GTK_LABEL(widgets->developers_lbl), text);
    g_free(temp);
    g_free(text);

    temp = stringArrayToString(vjuego->genres, MAX_CANT);
    text = g_strdup_printf("Generos: %s", temp);
    gtk_label_set_text(GTK_LABEL(widgets->genres_lbl), text);
    g_free(temp);
    g_free(text);

    temp = stringArrayToString(vjuego->publishers, MAX_CANT);
    text = g_strdup_printf("Distribuidoras: %s", temp);
    gtk_label_set_text(GTK_LABEL(widgets->publishers_lbl), text);
    g_free(temp);
    g_free(text);

    text = g_strdup_printf("Sitio web: %s", vjuego->website);
    gtk_label_set_text(GTK_LABEL(widgets->website_lbl), text);
    g_free(text);

    text = g_strdup_printf("Descripción: %s", vjuego->description);
    gtk_label_set_text(GTK_LABEL(widgets->description_lbl), text);
    g_free(text);
    
}

static void search_cb(GtkButton *btn, gpointer user_data){
    searchWidgets *widgets = user_data;
    GtkWidget *row;
    GtkWidget *name_lbl;
    Query q_name;
    Query q_publ;
    int count;

    GtkWidget *window = widgets->window;
    GtkWidget *result_list = widgets-> result_list;

    //Limpiar lista en cada búsqueda y liberar memoria  
    gtk_list_box_remove_all(GTK_LIST_BOX(result_list)); 
    if(widgets->results) g_free(widgets->results);

    widgets->results = NULL;
    widgets->result_count = 0;

    //Alocar nueva memoria 
    widgets->results = g_malloc(sizeof(Videojuego) * MAX_RESULTS);
    if(widgets->results == NULL){
        perror("Error alocando memoria");
        exit(-1);
    }

    Videojuego *results = widgets->results;

    const char *name_ent = gtk_editable_get_text(GTK_EDITABLE(widgets->name_entry));
    const char *publ_ent = gtk_editable_get_text(GTK_EDITABLE(widgets->publ_entry));
    
    char *name = g_strdup(name_ent); //Hacemos una copia mutable
    g_strstrip(name);//limpia espacios en blanco al inicio y al final
    char *publ = g_strdup(publ_ent);
    g_strstrip(publ);

    if(strlen(name) == 0 && strlen(publ) == 0){
        showError(GTK_WINDOW(window), "Ingrese al menos un criterio de búsqueda");
        g_free(name);
        g_free(publ);
        return;
    }

    if(strlen(name) > 0){
        //Solicitar busqueda por nombre
        q_name.type = 0;
        strcpy(q_name.criteria, name);

        writeFull(fd_query, &q_name, sizeof(q_name));
    } else if(strlen(publ) > 0){
        //Solicitar busqueda por distribuidora
        q_publ.type = 1;
        strcpy(q_publ.criteria, publ);

        writeFull(fd_query, &q_publ, sizeof(q_publ));
    }
    readFull(fd_response, &count, sizeof(int));
    if(count > MAX_RESULTS) count = MAX_RESULTS;
 
    if(count == 0){
        showError(GTK_WINDOW(widgets -> window), "No se encontraron resultados");
        g_free(name);
        g_free(publ);
        return;   
    } 

    readFull(fd_response, results, count * sizeof(Videojuego));

    /*NOTA. El buffer de la tubería es de solo 65536 bytes,
    así que debemos recibir los resultados por partes
    */

    //Mostrar resultados encontrados en pantalla
    
    for(int i = 0; i < count; i++){
        name_lbl = gtk_label_new(results[i].name); 
        row = gtk_list_box_row_new();

        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), name_lbl);
        gtk_list_box_append(GTK_LIST_BOX(result_list), row);

        //Asociar cada fila con su struct Videojuego correspondiente
        g_object_set_data(G_OBJECT(row), "game-data", &results[i]);
    }

    widgets->result_count = count;

    //Liberar memoria 
    g_free(name);
    g_free(publ);
}

static void activate(GtkApplication *app){//se construye toda la interfaz
    GtkWidget *window;
    GtkWidget *main_box;
    GtkWidget *search_box;
    GtkWidget *name_entry; //Para buscar por nombre
    GtkWidget *publ_entry; //Para buscar por distribuidora
    GtkWidget *search_btn; //Ejecutar la busqueda
    GtkWidget *paned; //Para dividir la ventana
    GtkWidget *result_list; //Mostrar resultados encontrados
    GtkWidget *result_list_scroll;
    GtkWidget *details_box; //Mostrar detalles de un resultado
    GtkWidget *details_scroll;

    GtkWidget *name_lbl;
    GtkWidget *release_date_lbl;
    GtkWidget *background_image_lbl;
    GtkWidget *rating_lbl;
    GtkWidget *ratings_count_lbl;
    GtkWidget *added_lbl;
    GtkWidget *playtime_lbl;
    GtkWidget *reviews_count_lbl;
    GtkWidget *platforms_lbl;
    GtkWidget *stores_lbl;
    GtkWidget *developers_lbl;
    GtkWidget *genres_lbl;
    GtkWidget *publishers_lbl;
    GtkWidget *website_lbl;
    GtkWidget *description_lbl;

    searchWidgets *s_widgets = g_malloc0(sizeof(searchWidgets));

    window = gtk_application_window_new (app);
    gtk_window_set_title (GTK_WINDOW (window), "Buscador de Videojuegos");
    gtk_window_set_default_size (GTK_WINDOW (window), 1000, 600);

    main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    gtk_window_set_child (GTK_WINDOW (window), main_box);
    
    search_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);

    // ----- Widgets de search_box ---------
    name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(name_entry), "Ingrese el nombre del juego");

    publ_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(publ_entry), "Ingrese la distribuidora del juego");
    
    search_btn = gtk_button_new_with_label("Buscar");

    s_widgets -> window = window;
    s_widgets -> name_entry = name_entry;
    s_widgets -> publ_entry = publ_entry;

    g_signal_connect (search_btn, "clicked", G_CALLBACK (search_cb), s_widgets);

    gtk_box_append(GTK_BOX(search_box), name_entry);
    gtk_box_append(GTK_BOX(search_box), publ_entry);
    gtk_box_append(GTK_BOX(search_box), search_btn);

    // Widgets de paned
    details_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 7);
    details_scroll = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(details_scroll, TRUE);
    gtk_widget_set_vexpand(details_scroll, TRUE);

    //Ajustar margenes
    gtk_widget_set_margin_start(details_box, 12);
    gtk_widget_set_margin_end(details_box, 12);
    gtk_widget_set_margin_top(details_box, 12);
    gtk_widget_set_margin_bottom(details_box, 12);

    result_list = gtk_list_box_new();
    s_widgets -> result_list = result_list;
    //Para mostrar detalles al seleccionar un resultado
    g_signal_connect(result_list, "row-selected", G_CALLBACK(mostrarDetalles), s_widgets); 
    result_list_scroll = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(result_list_scroll, TRUE);
    gtk_widget_set_vexpand(result_list_scroll, TRUE);

    name_lbl = gtk_label_new("");
    s_widgets -> name_lbl = name_lbl;

    release_date_lbl = gtk_label_new("");
    s_widgets -> release_date_lbl = release_date_lbl;

    background_image_lbl = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(background_image_lbl), TRUE);
    s_widgets -> background_image_lbl = background_image_lbl;

    rating_lbl = gtk_label_new("");
    s_widgets -> rating_lbl = rating_lbl;

    ratings_count_lbl = gtk_label_new("");
    s_widgets -> ratings_count_lbl = ratings_count_lbl;

    added_lbl = gtk_label_new("");
    s_widgets -> added_lbl = added_lbl;

    playtime_lbl = gtk_label_new("");
    s_widgets -> playtime_lbl = playtime_lbl;

    reviews_count_lbl = gtk_label_new("");
    s_widgets -> reviews_count_lbl = reviews_count_lbl;

    platforms_lbl = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(platforms_lbl), TRUE);
    s_widgets -> platforms_lbl = platforms_lbl;

    stores_lbl = gtk_label_new("");
    s_widgets -> stores_lbl = stores_lbl;

    developers_lbl = gtk_label_new("");
    s_widgets -> developers_lbl = developers_lbl;

    genres_lbl = gtk_label_new("");
    s_widgets -> genres_lbl = genres_lbl;

    publishers_lbl = gtk_label_new("");
    s_widgets -> publishers_lbl = publishers_lbl;

    website_lbl = gtk_label_new("");
    s_widgets -> website_lbl = website_lbl;
    
    description_lbl = gtk_label_new("");
    gtk_label_set_wrap(GTK_LABEL(description_lbl), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(description_lbl), PANGO_WRAP_WORD_CHAR);
    s_widgets -> description_lbl = description_lbl;

    gtk_box_append(GTK_BOX(details_box), name_lbl);
    gtk_box_append(GTK_BOX(details_box), release_date_lbl);
    gtk_box_append(GTK_BOX(details_box), background_image_lbl);
    gtk_box_append(GTK_BOX(details_box), rating_lbl);
    gtk_box_append(GTK_BOX(details_box), ratings_count_lbl);
    gtk_box_append(GTK_BOX(details_box), added_lbl);
    gtk_box_append(GTK_BOX(details_box), playtime_lbl);
    gtk_box_append(GTK_BOX(details_box), reviews_count_lbl);
    gtk_box_append(GTK_BOX(details_box), platforms_lbl);
    gtk_box_append(GTK_BOX(details_box), stores_lbl);
    gtk_box_append(GTK_BOX(details_box), developers_lbl);
    gtk_box_append(GTK_BOX(details_box), genres_lbl);
    gtk_box_append(GTK_BOX(details_box), publishers_lbl);
    gtk_box_append(GTK_BOX(details_box), website_lbl);
    gtk_box_append(GTK_BOX(details_box), description_lbl);

    gtk_widget_set_halign(name_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0);
    gtk_widget_set_halign(release_date_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(release_date_lbl), 0.0);
    gtk_widget_set_halign(background_image_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(background_image_lbl), 0.0);
    gtk_widget_set_halign(rating_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(rating_lbl), 0.0);
    gtk_widget_set_halign(ratings_count_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(ratings_count_lbl), 0.0);
    gtk_widget_set_halign(added_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(added_lbl), 0.0);
    gtk_widget_set_halign(playtime_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(playtime_lbl), 0.0);
    gtk_widget_set_halign(reviews_count_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(reviews_count_lbl), 0.0);
    gtk_widget_set_halign(platforms_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(platforms_lbl), 0.0);
    gtk_widget_set_halign(stores_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(stores_lbl), 0.0);
    gtk_widget_set_halign(developers_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(developers_lbl), 0.0);
    gtk_widget_set_halign(genres_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(genres_lbl), 0.0);
    gtk_widget_set_halign(publishers_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(publishers_lbl), 0.0);
    gtk_widget_set_halign(website_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(website_lbl), 0.0);
    gtk_widget_set_halign(description_lbl, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(description_lbl), 0.0);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(details_scroll), details_box);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(result_list_scroll), result_list);
    gtk_paned_set_start_child(GTK_PANED(paned), result_list_scroll);
    gtk_paned_set_end_child(GTK_PANED(paned), details_scroll);

    gtk_paned_set_position(GTK_PANED(paned), 450); //Definir posición de la separación
    
    gtk_box_append(GTK_BOX(main_box), search_box);
    gtk_box_append(GTK_BOX(main_box), paned);
    gtk_window_present(GTK_WINDOW (window));
}

int main(int argc, char **argv){
    GtkApplication *app;
    int status;

    fd_query = open("/tmp/fifo_query", O_WRONLY);
    if(fd_query < 0){
        perror("Error abriendo fifo_query");
        exit(-1);
    }

    fd_response = open("/tmp/fifo_response", O_RDONLY);
    if(fd_response < 0){
        perror("Error abriendo fifo_response");
        exit(-1);
    }

    app = gtk_application_new ("ui.gtk.practica1", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);

    g_object_unref(app); //Liberar app
    return status;
}



