#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include "sudoku_core.h"

static GtkWidget *status_label;
static GtkWidget *result_label;
static GtkWidget *sudoku_grid;
static GtkWidget *page_stack;

/* Dynamic CSS for cell font size (changes when the window is resized) */
static GtkCssProvider *cell_css;
static int last_font_px = 0;
static int pending_font_px = 0;

static void setup_css(void)
{
    GtkCssProvider *css = gtk_css_provider_new();

    const char *style =
        "window {"
        " background-color: #F3F6FC;"
        " font-family: 'Segoe UI', sans-serif;"
        "}"

        "#welcome-title {"
        " font-size: 30px;"
        " font-weight: bold;"
        " color: #15345B;"
        "}"

        "#welcome-subtitle {"
        " font-size: 19px;"
        " color: #243B57;"
        "}"

        ".section-heading {"
        " font-size: 23px;"
        " font-weight: bold;"
        " color: #15345B;"
        "}"

        ".rule-card {"
        " background-color: #FFFFFF;"
        " border: 1px solid #B9CBE0;"
        " border-radius: 10px;"
        " padding: 14px;"
        "}"

        ".rule-number {"
        " font-size: 25px;"
        " font-weight: bold;"
        " color: #174EA6;"
        "}"

        ".rule-text {"
        " font-size: 19px;"
        " font-weight: bold;"
        " color: #183153;"
        "}"

        ".rule-description {"
        " font-size: 17px;"
        " color: #27384D;"
        "}"

        ".notice {"
        " background-color: #E5EDF8;"
        " color: #183153;"
        " border: 1px solid #B9CBE0;"
        " border-radius: 8px;"
        " padding: 12px;"
        " font-size: 17px;"
        "}"

        ".primary-button {"
        " background-image: none;"
        " background-color: #E2E8F0;"
        " color: #183153;"
        " border: 2px solid #AABBD0;"
        " border-radius: 8px;"
        " padding: 12px 24px;"
        " font-size: 18px;"
        " font-weight: bold;"
        " box-shadow: none;"
        "}"

        ".primary-button label {"
        " color: #183153;"
        " font-size: 18px;"
        " font-weight: bold;"
        "}"

        ".primary-button:hover {"
        " background-image: none;"
        " background-color: #CBD8E8;"
        "}"

        ".primary-button:hover label {"
        " color: #183153;"
        "}"

        ".secondary-button {"
        " background-image: none;"
        " background-color: #E2E8F0;"
        " color: #1E293B;"
        " border: 1px solid #AABBD0;"
        " border-radius: 7px;"
        " padding: 8px 14px;"
        " font-size: 15px;"
        " font-weight: bold;"
        " box-shadow: none;"
        "}"

        ".secondary-button label { color: #1E293B; }"

        ".secondary-button:hover {"
        " background-image: none;"
        " background-color: #CBD5E1;"
        "}"

        ".success {"
        " background-color: #D1FAE5;"
        " color: #065F46;"
        " padding: 8px;"
        " font-size: 16px;"
        " font-weight: bold;"
        " border-radius: 6px;"
        "}"

        ".failure {"
        " background-color: #FEE2E2;"
        " color: #991B1B;"
        " padding: 8px;"
        " font-size: 16px;"
        " font-weight: bold;"
        " border-radius: 6px;"
        "}"

        ".neutral {"
        " color: #24364B;"
        " padding: 6px;"
        " font-size: 15px;"
        "}"

        /* Sudoku cells: no minimum size so they can shrink on small screens */
        "entry.sudoku-cell {"
        " font-weight: bold;"
        " border: 1px solid #8FA4BE;"
        " border-radius: 4px;"
        " padding: 0;"
        " min-width: 0;"
        " min-height: 0;"
        "}"

        ".block-a { background-color: #DCEBFA; color: #183153; }"
        ".block-b { background-color: #E4F4E7; color: #183153; }"
        ".block-c { background-color: #FFF0D5; color: #183153; }"

        "entry.sudoku-cell:focus {"
        " background-color: #FFF3A6;"
        " border: 2px solid #174EA6;"
        "}";

    gtk_css_provider_load_from_data(css, style, -1, NULL);

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    g_object_unref(css);

    /* Second provider, higher priority: only holds the cell font size */
    cell_css = gtk_css_provider_new();

    gtk_css_provider_load_from_data(
        cell_css, "entry.sudoku-cell { font-size: 20px; }", -1, NULL
    );

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(cell_css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION + 1
    );
}

static GtkWidget *make_label(
    const char *text,
    const char *css_class
)
{
    GtkWidget *label = gtk_label_new(text);

    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    if (css_class != NULL) {
        gtk_style_context_add_class(
            gtk_widget_get_style_context(label), css_class
        );
    }

    return label;
}

static GtkWidget *make_button(
    const char *text,
    const char *css_class
)
{
    GtkWidget *button = gtk_button_new_with_label(text);

    gtk_style_context_add_class(
        gtk_widget_get_style_context(button), css_class
    );

    return button;
}

static void set_status(
    const char *message,
    const char *css_class
)
{
    GtkStyleContext *context =
        gtk_widget_get_style_context(status_label);

    gtk_style_context_remove_class(context, "success");
    gtk_style_context_remove_class(context, "failure");
    gtk_style_context_remove_class(context, "neutral");

    gtk_style_context_add_class(context, css_class);
    gtk_label_set_text(GTK_LABEL(status_label), message);
}

/* ---------- Responsive font handling ---------- */

static gboolean apply_font_idle(gpointer data)
{
    (void)data;

    if (pending_font_px == last_font_px) {
        return G_SOURCE_REMOVE;
    }

    last_font_px = pending_font_px;

    char css[80];

    g_snprintf(
        css, sizeof(css),
        "entry.sudoku-cell { font-size: %dpx; }", last_font_px
    );

    gtk_css_provider_load_from_data(cell_css, css, -1, NULL);

    return G_SOURCE_REMOVE;
}

/* Connected to the grid itself, so alloc is the real square size */
static void on_grid_size_allocate(
    GtkWidget *widget,
    GdkRectangle *alloc,
    gpointer user_data
)
{
    (void)widget;
    (void)user_data;

    int side = MIN(alloc->width, alloc->height);
    int cell = side / SIZE;
    int font = CLAMP((int)(cell * 0.5), 12, 48);

    if (font != pending_font_px) {
        pending_font_px = font;
        g_idle_add(apply_font_idle, NULL);
    }
}

/* ---------- Callbacks ---------- */

/* Enter дарахад дараагийн нүд рүү шилжинэ (мөр мөрөөр, сүүлийн нүднээс эхний нүд рүү) */
static void on_cell_activate(
    GtkEntry *entry,
    gpointer user_data
)
{
    (void)entry;

    int index = GPOINTER_TO_INT(user_data);
    int next = (index + 1) % (SIZE * SIZE);

    GtkWidget *next_entry = gtk_grid_get_child_at(
        GTK_GRID(sudoku_grid), next % SIZE, next / SIZE
    );

    if (next_entry != NULL) {
        gtk_widget_grab_focus(next_entry);
    }
}

static void on_start_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    (void)button;
    (void)user_data;

    gtk_stack_set_visible_child_name(
        GTK_STACK(page_stack), "game"
    );
}

static void on_instructions_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    (void)button;
    (void)user_data;

    gtk_stack_set_visible_child_name(
        GTK_STACK(page_stack), "instructions"
    );
}

static void on_check_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    (void)button;
    (void)user_data;

    gtk_label_set_text(GTK_LABEL(result_label), "");

    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            GtkWidget *entry = gtk_grid_get_child_at(
                GTK_GRID(sudoku_grid), col, row
            );

            const char *text =
                gtk_entry_get_text(GTK_ENTRY(entry));

            if (text == NULL || text[0] == '\0') {
                char message[128];

                g_snprintf(
                    message, sizeof(message),
                    "Алдаа: %d-р мөр, %d-р багана хоосон байна.",
                    row + 1, col + 1
                );

                set_status(message, "failure");
                gtk_widget_grab_focus(entry);
                return;
            }

            if (strlen(text) != 1 ||
                !isdigit((unsigned char)text[0])) {
                char message[128];

                g_snprintf(
                    message, sizeof(message),
                    "Алдаа: %d-р мөр, %d-р баганад 1–9 оруулна.",
                    row + 1, col + 1
                );

                set_status(message, "failure");
                gtk_widget_grab_focus(entry);
                return;
            }

            int value = text[0] - '0';

            if (value < 1 || value > 9) {
                char message[128];

                g_snprintf(
                    message, sizeof(message),
                    "Алдаа: %d-р мөр, %d-р баганад 1–9 оруулна.",
                    row + 1, col + 1
                );

                set_status(message, "failure");
                gtk_widget_grab_focus(entry);
                return;
            }

            sudoku[row][col] = value;
        }
    }

    set_status("Sudoku-г шалгаж байна...", "neutral");

    int validation = validate_sudoku();

    if (validation == -1) {
        set_status(
            "Алдаа: шалгах thread ажиллуулах боломжгүй байна.",
            "failure"
        );
        return;
    }

    if (validation == 1) {
        set_status("Амжилттай! Sudoku зөв байна.", "success");
    } else {
        set_status(
            "Sudoku буруу байна. FAIL үр дүнг шалгана уу.",
            "failure"
        );
    }

    GString *details = g_string_new("");

    g_string_append_printf(
        details, "Мөрүүд: %s\n",
        results[0] ? "PASS" : "FAIL"
    );

    g_string_append_printf(
        details, "Баганууд: %s\n",
        results[1] ? "PASS" : "FAIL"
    );

    g_string_append(details, "3×3 дэд хүснэгтүүд:\n");

    for (int i = 0; i < 9; i++) {
        g_string_append_printf(
            details, "%s",
            results[i + 2] ? "PASS" : "FAIL"
        );

        if (i % 3 == 2) {
            g_string_append(details, "\n");
        } else {
            g_string_append(details, "   ");
        }
    }

    gtk_label_set_text(
        GTK_LABEL(result_label), details->str
    );

    g_string_free(details, TRUE);
}

static void on_clear_clicked(
    GtkButton *button,
    gpointer user_data
)
{
    (void)button;
    (void)user_data;

    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            GtkWidget *entry = gtk_grid_get_child_at(
                GTK_GRID(sudoku_grid), col, row
            );

            if (entry != NULL) {
                gtk_entry_set_text(GTK_ENTRY(entry), "");
            }
        }
    }

    set_status("Үр дүн: Шалгаагүй", "neutral");
    gtk_label_set_text(GTK_LABEL(result_label), "");
}

/* ---------- Instructions page ---------- */

static GtkWidget *create_rule_card(
    const char *number,
    const char *heading,
    const char *description
)
{
    GtkWidget *card = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, 16
    );

    gtk_style_context_add_class(
        gtk_widget_get_style_context(card), "rule-card"
    );

    GtkWidget *number_label = make_label(
        number, "rule-number"
    );

    GtkWidget *text_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL, 6
    );

    GtkWidget *title = make_label(
        heading, "rule-text"
    );

    GtkWidget *detail = make_label(
        description, "rule-description"
    );

    gtk_box_pack_start(
        GTK_BOX(text_box), title, FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(text_box), detail, FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(card), number_label, FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(card), text_box, TRUE, TRUE, 0
    );

    return card;
}

/* Зааврын агуулга */
static GtkWidget *create_instructions_content(void)
{
    GtkWidget *page = gtk_box_new(
        GTK_ORIENTATION_VERTICAL, 16
    );

    gtk_container_set_border_width(GTK_CONTAINER(page), 24);

    GtkWidget *title = make_label(
        "SUDOKU ТОГЛОХ ЗААВАР", NULL
    );

    gtk_widget_set_name(title, "welcome-title");
    gtk_label_set_xalign(GTK_LABEL(title), 0.5);

    GtkWidget *subtitle = make_label(
        "Дүрэмтэй танилцаад тоглоомоо эхлүүлээрэй.",
        NULL
    );

    gtk_widget_set_name(subtitle, "welcome-subtitle");
    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.5);

    gtk_box_pack_start(
        GTK_BOX(page), title, FALSE, FALSE, 4
    );

    gtk_box_pack_start(
        GTK_BOX(page), subtitle, FALSE, FALSE, 0
    );

    GtkWidget *heading = make_label(
        "ХЭРХЭН ТОГЛОХ ВЭ?", "section-heading"
    );

    gtk_box_pack_start(
        GTK_BOX(page), heading, FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(page),
        create_rule_card(
            "1", "Нүд бүрд цифр оруулна",
            "9×9 хүснэгтийн нүд бүрд 1–9 хүртэлх нэг цифр бичнэ."
        ),
        FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(page),
        create_rule_card(
            "2", "Мөр ба багана",
            "Нэг мөр эсвэл нэг баганад ижил цифр давхардаж болохгүй."
        ),
        FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(page),
        create_rule_card(
            "3", "3×3 хэсэг",
            "Өнгөөр ялгасан 3×3 дэд хүснэгт бүрд цифр давхардахгүй."
        ),
        FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(page),
        create_rule_card(
            "4", "Хоосон нүд",
            "Нүд хоосон байвал програм тухайн нүдний байрлалыг мэдээлнэ."
        ),
        FALSE, FALSE, 0
    );

    GtkWidget *notice = make_label(
        "САНАМЖ: PASS нь зөв, FAIL нь алдаа байгааг илэрхийлнэ.",
        "notice"
    );

    gtk_box_pack_start(
        GTK_BOX(page), notice, FALSE, FALSE, 0
    );

    return page;
}

/* Эхлэх товч доод хэсэгт тогтмол байрлана */
static GtkWidget *create_instructions_page(void)
{
    GtkWidget *page = gtk_box_new(
        GTK_ORIENTATION_VERTICAL, 0
    );

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER,
        GTK_POLICY_AUTOMATIC
    );

    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);

    gtk_container_add(
        GTK_CONTAINER(scroll), create_instructions_content()
    );

    gtk_box_pack_start(
        GTK_BOX(page), scroll, TRUE, TRUE, 0
    );

    GtkWidget *footer = gtk_box_new(
        GTK_ORIENTATION_VERTICAL, 8
    );

    gtk_container_set_border_width(GTK_CONTAINER(footer), 12);

    GtkWidget *start_button = make_button(
        "ТОГЛООМ ЭХЛЭХ", "primary-button"
    );

    gtk_widget_set_halign(
        start_button, GTK_ALIGN_CENTER
    );

    gtk_widget_set_hexpand(start_button, TRUE);

    gtk_box_pack_start(
        GTK_BOX(footer), start_button, FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(page), footer, FALSE, FALSE, 0
    );

    g_signal_connect(
        start_button, "clicked",
        G_CALLBACK(on_start_clicked), NULL
    );

    return page;
}

/* ---------- Game page ---------- */

static GtkWidget *create_game_page(void)
{
    GtkWidget *page = gtk_box_new(
        GTK_ORIENTATION_VERTICAL, 8
    );

    gtk_container_set_border_width(GTK_CONTAINER(page), 12);

    GtkWidget *title = make_label(
        "Sudoku шалгах програм", "section-heading"
    );

    gtk_label_set_xalign(GTK_LABEL(title), 0.5);

    GtkWidget *subtitle = make_label(
        "Нүд бүрд 1–9 хүртэлх нэг цифр оруулна.",
        "rule-description"
    );

    gtk_label_set_xalign(GTK_LABEL(subtitle), 0.5);

    gtk_box_pack_start(
        GTK_BOX(page), title, FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(page), subtitle, FALSE, FALSE, 0
    );

    /* 1:1 aspect frame: the grid always stays square and fully visible */
    GtkWidget *grid_frame =
        gtk_aspect_frame_new(NULL, 0.5, 0.5, 1.0, FALSE);

    gtk_frame_set_shadow_type(
        GTK_FRAME(grid_frame), GTK_SHADOW_NONE
    );

    gtk_widget_set_hexpand(grid_frame, TRUE);
    gtk_widget_set_vexpand(grid_frame, TRUE);
    gtk_widget_set_size_request(grid_frame, 300, 300);

    sudoku_grid = gtk_grid_new();

    gtk_widget_set_hexpand(sudoku_grid, TRUE);
    gtk_widget_set_vexpand(sudoku_grid, TRUE);

    gtk_grid_set_row_spacing(GTK_GRID(sudoku_grid), 2);
    gtk_grid_set_column_spacing(GTK_GRID(sudoku_grid), 2);

    gtk_grid_set_row_homogeneous(GTK_GRID(sudoku_grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(sudoku_grid), TRUE);

    for (int row = 0; row < SIZE; row++) {
        for (int col = 0; col < SIZE; col++) {
            GtkWidget *entry = gtk_entry_new();

            gtk_entry_set_max_length(GTK_ENTRY(entry), 1);
            gtk_entry_set_alignment(GTK_ENTRY(entry), 0.5);

            /* Default entry width (~20 chars) is what stretched the cells */
            gtk_entry_set_width_chars(GTK_ENTRY(entry), 1);
            gtk_entry_set_max_width_chars(GTK_ENTRY(entry), 1);

            gtk_widget_set_hexpand(entry, TRUE);
            gtk_widget_set_vexpand(entry, TRUE);

            GtkStyleContext *ctx =
                gtk_widget_get_style_context(entry);

            gtk_style_context_add_class(ctx, "sudoku-cell");

            int block = (row / 3) * 3 + (col / 3);
            const char *class_name;

            if (block == 4) {
                class_name = "block-c";
            } else if (block % 2 == 0) {
                class_name = "block-a";
            } else {
                class_name = "block-b";
            }

            gtk_style_context_add_class(ctx, class_name);

            /* Enter дарахад дараагийн нүд рүү шилжинэ */
            g_signal_connect(
                entry, "activate",
                G_CALLBACK(on_cell_activate),
                GINT_TO_POINTER(row * SIZE + col)
            );

            gtk_grid_attach(
                GTK_GRID(sudoku_grid),
                entry, col, row, 1, 1
            );
        }
    }

    gtk_container_add(GTK_CONTAINER(grid_frame), sudoku_grid);

    g_signal_connect(
        sudoku_grid, "size-allocate",
        G_CALLBACK(on_grid_size_allocate), NULL
    );

    gtk_box_pack_start(
        GTK_BOX(page), grid_frame, TRUE, TRUE, 4
    );

    GtkWidget *buttons = gtk_box_new(
        GTK_ORIENTATION_HORIZONTAL, 8
    );

    gtk_widget_set_halign(buttons, GTK_ALIGN_CENTER);

    GtkWidget *check_button = make_button(
        "Шалгах", "secondary-button"
    );

    GtkWidget *clear_button = make_button(
        "Цэвэрлэх", "secondary-button"
    );

    GtkWidget *back_button = make_button(
        "Заавар", "secondary-button"
    );

    gtk_box_pack_start(
        GTK_BOX(buttons), check_button, FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(buttons), clear_button, FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(buttons), back_button, FALSE, FALSE, 0
    );

    gtk_box_pack_start(
        GTK_BOX(page), buttons, FALSE, FALSE, 2
    );

    status_label = gtk_label_new("Үр дүн: Шалгаагүй");

    gtk_label_set_line_wrap(GTK_LABEL(status_label), TRUE);

    gtk_style_context_add_class(
        gtk_widget_get_style_context(status_label), "neutral"
    );

    gtk_box_pack_start(
        GTK_BOX(page), status_label, FALSE, FALSE, 0
    );

    result_label = gtk_label_new("");

    gtk_label_set_line_wrap(GTK_LABEL(result_label), TRUE);

    gtk_box_pack_start(
        GTK_BOX(page), result_label, FALSE, FALSE, 0
    );

    g_signal_connect(
        check_button, "clicked",
        G_CALLBACK(on_check_clicked), NULL
    );

    g_signal_connect(
        clear_button, "clicked",
        G_CALLBACK(on_clear_clicked), NULL
    );

    g_signal_connect(
        back_button, "clicked",
        G_CALLBACK(on_instructions_clicked), NULL
    );

    return page;
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);
    setup_css();

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(
        GTK_WINDOW(window), "Sudoku Validator"
    );

    gtk_window_set_default_size(
        GTK_WINDOW(window), 700, 760
    );

    /* Minimum size so small screens never break the layout */
    gtk_widget_set_size_request(window, 420, 560);

    gtk_window_set_position(
        GTK_WINDOW(window), GTK_WIN_POS_CENTER
    );

    gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

    gtk_container_set_border_width(
        GTK_CONTAINER(window), 8
    );

    page_stack = gtk_stack_new();

    gtk_stack_set_transition_type(
        GTK_STACK(page_stack),
        GTK_STACK_TRANSITION_TYPE_CROSSFADE
    );

    gtk_stack_set_transition_duration(
        GTK_STACK(page_stack), 200
    );

    gtk_stack_add_named(
        GTK_STACK(page_stack),
        create_instructions_page(),
        "instructions"
    );

    gtk_stack_add_named(
        GTK_STACK(page_stack),
        create_game_page(),
        "game"
    );

    gtk_container_add(
        GTK_CONTAINER(window), page_stack
    );

    gtk_stack_set_visible_child_name(
        GTK_STACK(page_stack), "instructions"
    );

    g_signal_connect(
        window, "destroy",
        G_CALLBACK(gtk_main_quit), NULL
    );

    gtk_widget_show_all(window);
    gtk_main();

    return 0;
}
