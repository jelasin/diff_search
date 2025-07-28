#include <gtk/gtk.h>
#include "../lib/cJSON/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <glib.h>

typedef struct {
    char *md5;
    char *file1_path;
    char *file2_path;
    char *status;
} DiffEntry;

typedef struct {
    GtkWidget *window;
    GtkWidget *search_entry;
    GtkWidget *file1_tree;
    GtkWidget *file2_tree;
    GtkListStore *file1_store;
    GtkListStore *file2_store;
    GArray *diff_entries;
    GHashTable *unique_entries; // 用于去重
} AppData;

enum {
    COL_FILENAME = 0,
    COL_MD5,
    COL_STATUS,
    N_COLUMNS
};

// 释放DiffEntry
static void free_diff_entry(DiffEntry *entry) {
    if (entry) {
        g_free(entry->md5);
        g_free(entry->file1_path);
        g_free(entry->file2_path);
        g_free(entry->status);
    }
}

// 创建文件树视图
static GtkWidget *create_file_tree(GtkListStore **store) {
    GtkWidget *tree_view;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;

    // 创建模型
    *store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    
    // 创建树视图
    tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(*store));
    
    // 文件名列
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("文件名", renderer, "text", COL_FILENAME, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COL_FILENAME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    // MD5列
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("MD5", renderer, "text", COL_MD5, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    // 状态列
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("状态", renderer, "text", COL_STATUS, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    return tree_view;
}

// 解析JSON文件
static gboolean parse_diff_json(const char *filename, AppData *app_data) {
    FILE *file;
    char *buffer;
    long file_size;
    cJSON *root, *diff_array, *entry_obj;
    cJSON *md5_obj, *file1_obj, *file2_obj, *status_obj;
    int array_len;

    // 读取文件
    file = fopen(filename, "r");
    if (!file) {
        g_warning("无法打开文件: %s", filename);
        return FALSE;
    }

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    buffer = g_malloc(file_size + 1);
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';
    fclose(file);

    // 解析JSON
    root = cJSON_Parse(buffer);
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            g_warning("JSON解析失败，错误位置: %s", error_ptr);
        } else {
            g_warning("JSON解析失败");
        }
        g_free(buffer);
        return FALSE;
    }
    g_free(buffer);

    diff_array = cJSON_GetObjectItem(root, "files");
    if (!diff_array) {
        g_warning("找不到files对象");
        cJSON_Delete(root);
        return FALSE;
    }
    
    if (!cJSON_IsArray(diff_array)) {
        g_warning("files不是数组类型");
        cJSON_Delete(root);
        return FALSE;
    }

    array_len = cJSON_GetArraySize(diff_array);
    
    for (int i = 0; i < (int)array_len; i++) {
        entry_obj = cJSON_GetArrayItem(diff_array, i);
        
        md5_obj = cJSON_GetObjectItem(entry_obj, "md5");
        file1_obj = cJSON_GetObjectItem(entry_obj, "file1_path");
        file2_obj = cJSON_GetObjectItem(entry_obj, "file2_path");
        status_obj = cJSON_GetObjectItem(entry_obj, "status");
        
        if (md5_obj && file1_obj && file2_obj && status_obj &&
            cJSON_IsString(md5_obj) && cJSON_IsString(file1_obj) && 
            cJSON_IsString(file2_obj) && cJSON_IsString(status_obj)) {
            
            const char *md5 = cJSON_GetStringValue(md5_obj);
            const char *file1_path = cJSON_GetStringValue(file1_obj);
            const char *file2_path = cJSON_GetStringValue(file2_obj);
            const char *status = cJSON_GetStringValue(status_obj);

            // 创建唯一键用于去重 (md5 + file1_path + file2_path)
            char *unique_key = g_strdup_printf("%s|%s|%s", md5, file1_path, file2_path);
            
            // 检查是否已存在
            if (!g_hash_table_contains(app_data->unique_entries, unique_key)) {
                DiffEntry entry;
                entry.md5 = g_strdup(md5);
                entry.file1_path = g_strdup(file1_path);
                entry.file2_path = g_strdup(file2_path);
                entry.status = g_strdup(status);
                
                g_array_append_val(app_data->diff_entries, entry);
                g_hash_table_insert(app_data->unique_entries, unique_key, GINT_TO_POINTER(1));
            } else {
                g_free(unique_key);
            }
        }
    }

    cJSON_Delete(root);
    return TRUE;
}

// 更新文件树显示
static void update_file_trees(AppData *app_data, const char *search_text) {
    GtkTreeIter iter;
    
    // 清空现有数据
    gtk_list_store_clear(app_data->file1_store);
    gtk_list_store_clear(app_data->file2_store);
    
    for (int i = 0; i < (int)app_data->diff_entries->len; i++) {
        DiffEntry *entry = &g_array_index(app_data->diff_entries, DiffEntry, i);
        
        // 搜索过滤
        gboolean show_file1 = TRUE, show_file2 = TRUE;
        if (search_text && strlen(search_text) > 0) {
            if (strlen(entry->file1_path) > 0) {
                show_file1 = (strstr(entry->file1_path, search_text) != NULL);
            } else {
                show_file1 = FALSE;
            }
            
            if (strlen(entry->file2_path) > 0) {
                show_file2 = (strstr(entry->file2_path, search_text) != NULL);
            } else {
                show_file2 = FALSE;
            }
        }
        
        // 添加到file1树
        if (strlen(entry->file1_path) > 0 && show_file1) {
            gtk_list_store_append(app_data->file1_store, &iter);
            gtk_list_store_set(app_data->file1_store, &iter,
                COL_FILENAME, entry->file1_path,
                COL_MD5, entry->md5,
                COL_STATUS, entry->status,
                -1);
        }
        
        // 添加到file2树
        if (strlen(entry->file2_path) > 0 && show_file2) {
            gtk_list_store_append(app_data->file2_store, &iter);
            gtk_list_store_set(app_data->file2_store, &iter,
                COL_FILENAME, entry->file2_path,
                COL_MD5, entry->md5,
                COL_STATUS, entry->status,
                -1);
        }
    }
}

// 搜索回调函数
static void on_search_changed(GtkEntry *entry, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    const char *search_text = gtk_entry_get_text(entry);
    update_file_trees(app_data, search_text);
}

// 打开文件对话框
static void on_open_file(GtkWidget *widget, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    GtkWidget *dialog;
    GtkFileFilter *filter;
    
    (void)widget; // 消除未使用参数警告
    
    dialog = gtk_file_chooser_dialog_new("打开diff.json文件",
                                        GTK_WINDOW(app_data->window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_取消", GTK_RESPONSE_CANCEL,
                                        "_打开", GTK_RESPONSE_ACCEPT,
                                        NULL);
    
    // 添加文件过滤器
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "JSON files");
    gtk_file_filter_add_pattern(filter, "*.json");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);
    
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        
        // 清空现有数据
        for (int i = 0; i < (int)app_data->diff_entries->len; i++) {
            DiffEntry *entry = &g_array_index(app_data->diff_entries, DiffEntry, i);
            free_diff_entry(entry);
        }
        g_array_set_size(app_data->diff_entries, 0);
        g_hash_table_remove_all(app_data->unique_entries);
        
        // 解析新文件
        if (parse_diff_json(filename, app_data)) {
            update_file_trees(app_data, NULL);
            g_print("成功加载文件: %s (去重后共%d条记录)\n", filename, app_data->diff_entries->len);
        }
        
        g_free(filename);
    }
    
    gtk_widget_destroy(dialog);
}

// 关于对话框
static void on_about(GtkWidget *widget, gpointer user_data) {
    GtkWidget *dialog;
    
    (void)widget; // 消除未使用参数警告
    (void)user_data; // 消除未使用参数警告
    
    dialog = gtk_about_dialog_new();
    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "Diff Viewer");
    gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "1.0");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), "用于查看diff.json文件的GUI工具");
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

// 创建菜单栏
static GtkWidget *create_menubar(AppData *app_data) {
    GtkWidget *menubar;
    GtkWidget *file_menu, *help_menu;
    GtkWidget *file_mi, *help_mi;
    GtkWidget *open_mi, *quit_mi, *about_mi;
    
    menubar = gtk_menu_bar_new();
    
    // 文件菜单
    file_menu = gtk_menu_new();
    file_mi = gtk_menu_item_new_with_label("文件");
    
    open_mi = gtk_menu_item_new_with_label("打开");
    g_signal_connect(open_mi, "activate", G_CALLBACK(on_open_file), app_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_mi);
    
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), gtk_separator_menu_item_new());
    
    quit_mi = gtk_menu_item_new_with_label("退出");
    g_signal_connect(quit_mi, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_mi);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_mi), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_mi);
    
    // 帮助菜单
    help_menu = gtk_menu_new();
    help_mi = gtk_menu_item_new_with_label("帮助");
    
    about_mi = gtk_menu_item_new_with_label("关于");
    g_signal_connect(about_mi, "activate", G_CALLBACK(on_about), app_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), about_mi);
    
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(help_mi), help_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help_mi);
    
    return menubar;
}

// 程序退出时的清理
static void cleanup_app_data(AppData *app_data) {
    // 释放diff_entries数组
    for (int i = 0; i < (int)app_data->diff_entries->len; i++) {
        DiffEntry *entry = &g_array_index(app_data->diff_entries, DiffEntry, i);
        free_diff_entry(entry);
    }
    g_array_free(app_data->diff_entries, TRUE);
    
    // 释放哈希表
    g_hash_table_destroy(app_data->unique_entries);
}

int main(int argc, char *argv[]) {
    AppData app_data = {0};
    GtkWidget *vbox, *search_hbox;
    GtkWidget *search_label;
    GtkWidget *paned;
    GtkWidget *scrolled1, *scrolled2;
    GtkWidget *frame1, *frame2;
    GtkWidget *menubar;
    
    gtk_init(&argc, &argv);
    
    // 初始化应用数据
    app_data.diff_entries = g_array_new(FALSE, FALSE, sizeof(DiffEntry));
    app_data.unique_entries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    
    // 创建主窗口
    app_data.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app_data.window), "Diff Viewer");
    gtk_window_set_default_size(GTK_WINDOW(app_data.window), 1200, 800);
    gtk_container_set_border_width(GTK_CONTAINER(app_data.window), 5);
    g_signal_connect(app_data.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    
    // 创建垂直布局容器
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(app_data.window), vbox);
    
    // 创建菜单栏
    menubar = create_menubar(&app_data);
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);
    
    // 创建搜索区域
    search_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), search_hbox, FALSE, FALSE, 0);
    
    search_label = gtk_label_new("搜索文件名:");
    gtk_box_pack_start(GTK_BOX(search_hbox), search_label, FALSE, FALSE, 0);
    
    app_data.search_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app_data.search_entry), "输入文件名进行搜索...");
    g_signal_connect(app_data.search_entry, "changed", G_CALLBACK(on_search_changed), &app_data);
    gtk_box_pack_start(GTK_BOX(search_hbox), app_data.search_entry, TRUE, TRUE, 0);
    
    // 创建水平分割面板
    paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), paned, TRUE, TRUE, 0);
    
    // 创建左侧框架 (File1)
    frame1 = gtk_frame_new("目录1 (file1_path)");
    scrolled1 = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled1), 
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    app_data.file1_tree = create_file_tree(&app_data.file1_store);
    gtk_container_add(GTK_CONTAINER(scrolled1), app_data.file1_tree);
    gtk_container_add(GTK_CONTAINER(frame1), scrolled1);
    gtk_paned_pack1(GTK_PANED(paned), frame1, TRUE, FALSE);
    
    // 创建右侧框架 (File2)
    frame2 = gtk_frame_new("目录2 (file2_path)");
    scrolled2 = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled2), 
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    app_data.file2_tree = create_file_tree(&app_data.file2_store);
    gtk_container_add(GTK_CONTAINER(scrolled2), app_data.file2_tree);
    gtk_container_add(GTK_CONTAINER(frame2), scrolled2);
    gtk_paned_pack2(GTK_PANED(paned), frame2, TRUE, FALSE);
    
    // 设置分割面板位置
    gtk_paned_set_position(GTK_PANED(paned), 600);
    
    // 显示所有组件
    gtk_widget_show_all(app_data.window);
    
    // 如果提供了命令行参数，尝试打开文件
    if (argc > 1) {
        if (parse_diff_json(argv[1], &app_data)) {
            update_file_trees(&app_data, NULL);
            g_print("成功加载文件: %s (去重后共%d条记录)\n", argv[1], app_data.diff_entries->len);
        }
    }
    
    // 运行主循环
    gtk_main();
    
    // 清理资源
    cleanup_app_data(&app_data);
    
    return 0;
}
