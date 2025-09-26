// build Windows:  g++ -std=c++20 main.cc `pkg-config --cflags --libs gtkmm-4.0 libgda-5.0 ` -o app -DUSE_LIBGDA6
// must install:  pacman -S mingw-w64-x86_64-libgda
// build Debian13: g++ -std=c++20 main.cc `pkg-config --cflags --libs gtkmm-4.0 libgda-5.0 ` -o app

#include <gtkmm.h>
#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include <sstream>
#include <iomanip>
#include <glib-object.h>
#include <libgda/libgda.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-value.h>

//#define USE_POSTGRESQL   // Not tested

class DatabaseManager {
public:

    struct TableData {
        std::vector<std::string> column_names;
        std::vector<std::vector<std::string>> rows_data;
    };

    DatabaseManager(const std::string& provider_name, const std::string& connection_string)
        : m_provider_name(provider_name), m_connection_string(connection_string), m_column_selection("*"), m_order_by("") {
        std::string table_name = "data_types";
        m_column_names = get_column_names_internal(table_name);
        m_column_types = get_column_types_internal(table_name);
    }


    TableData get_paged_table_data(const std::string& table_name, gint limit, gint offset) {
        TableData result;
        GdaConnection* connection = get_db_connection();
        if (!connection) {
            return result;
        }

        std::stringstream query_stream;
        query_stream << "SELECT " << m_column_selection << " FROM " << table_name;
        if (!m_order_by.empty()) {
            query_stream << " " << m_order_by;
        }
        query_stream << " LIMIT " << limit << " OFFSET " << offset;

        GError* execute_error = NULL;
        GdaDataModel* model = gda_connection_execute_select_command(
            connection,
            query_stream.str().c_str(),
            &execute_error
        );

        if (model) {
            gint n_rows = gda_data_model_get_n_rows(model);
            gint n_cols = gda_data_model_get_n_columns(model);

            for (gint j = 0; j < n_cols; ++j) {
                const gchar* name = gda_data_model_get_column_name(model, j);
                if (name) {
                    result.column_names.emplace_back(name);
                }
            }

            for (gint i = 0; i < n_rows; ++i) {
                std::vector<std::string> row_data;
                for (gint j = 0; j < n_cols; ++j) {
                    const GValue* value = gda_data_model_get_value_at(model, j, i, NULL);
                    std::string str_val;

                    if (value && !gda_value_is_null(value)) {
                        gchar* general_str = g_strdup_value_contents(value);
                        if (general_str) {
                            str_val = general_str;
                            g_free(general_str);
                        }
                    } else {
                        str_val = "";
                    }

                    if (!result.column_names.empty() && result.column_names.size() > j) {
                        const std::string& column_name = result.column_names[j];
                        if (column_name == "amount") {
                            try {
                                double amount_val = std::stod(str_val);
                                std::stringstream ss;
                                ss << std::fixed << std::setprecision(2) << amount_val;
                                str_val = ss.str();
                            } catch (const std::exception& e) {
                            }
                        } else if (column_name == "name" && str_val.size() >= 2 && str_val.front() == '"' && str_val.back() == '"') {
                            str_val = str_val.substr(1, str_val.size() - 2);
                        } else if (column_name == "creation_date" && str_val.size() >= 10) {
                            str_val = str_val.substr(0, 10);
                        } else if (column_name == "creation_time" && str_val.size() >= 8) {
                            str_val = str_val.substr(0, 8);
                        }
                    }

                    row_data.emplace_back(str_val);
                }
                result.rows_data.push_back(row_data);
            }
            g_object_unref(model);
        } else if (execute_error) {
            std::cerr << "Error get data from table: " << execute_error->message << std::endl;
            g_error_free(execute_error);
        }

#ifdef USE_LIBGDA6
        gda_connection_close(connection, NULL);
#else
        gda_connection_close(connection);
#endif
        g_object_unref(connection);
        return result;
    }


    const std::vector<std::string>& get_cached_column_names() const {
        return m_column_names;
    }


    const std::vector<std::string>& get_cached_column_types() const {
        return m_column_types;
    }


    void set_column_selection(const std::string& selection) {
        m_column_selection = selection;
    }


    void set_order_by(const std::string& order_by_clause) {
        m_order_by = order_by_clause;
    }

private:
    std::string m_provider_name;
    std::string m_connection_string;
    std::vector<std::string> m_column_names;
    std::vector<std::string> m_column_types;
    std::string m_column_selection;
    std::string m_order_by;


    GdaConnection* get_db_connection() {
        gda_init();
        GError* open_error = NULL;

        GdaConnection* connection = gda_connection_open_from_string(
            m_provider_name.c_str(),
            m_connection_string.c_str(),
            NULL,
            GDA_CONNECTION_OPTIONS_NONE,
            &open_error
        );

        if (open_error) {
            std::cerr << "Error open/create database: " << open_error->message << std::endl;
            g_error_free(open_error);
            return nullptr;
        }

        return connection;
    }

    std::vector<std::string> get_column_names_internal(const std::string& table_name) {
        std::vector<std::string> column_names;
        GdaConnection* connection = get_db_connection();
        if (!connection) {
            return column_names;
        }

        GError* error = NULL;
        GdaDataModel* model = gda_connection_execute_select_command(
            connection,
            g_strdup_printf("SELECT * FROM %s LIMIT 1", table_name.c_str()),
            &error
        );

        if (model) {
            gint n_cols = gda_data_model_get_n_columns(model);
            for (gint j = 0; j < n_cols; ++j) {
                const gchar* name = gda_data_model_get_column_name(model, j);
                if (name) {
                    column_names.emplace_back(name);
                }
            }
            g_object_unref(model);
        } else if (error) {
            std::cerr << "Error get names of columns: " << error->message << std::endl;
            g_error_free(error);
        }

#ifdef USE_LIBGDA6
        gda_connection_close(connection, NULL);
#else
        gda_connection_close(connection);
#endif
        g_object_unref(connection);
        return column_names;
    }


    std::vector<std::string> get_column_types_internal(const std::string& table_name) {
        std::vector<std::string> column_types;
        GdaConnection* connection = get_db_connection();
        if (!connection) {
            return column_types;
        }

        GError* error = NULL;
        GdaDataModel* model = gda_connection_execute_select_command(
            connection,
            g_strdup_printf("SELECT * FROM %s LIMIT 1", table_name.c_str()),
            &error
        );

        if (model) {
            gint n_cols = gda_data_model_get_n_columns(model);
            for (gint j = 0; j < n_cols; ++j) {
                const GValue* value = gda_data_model_get_value_at(model, j, 0, NULL);
                if (value) {
                    const gchar* type_name = g_type_name(G_VALUE_TYPE(value));
                    if (type_name) {
                        column_types.emplace_back(type_name);
                    }
                }
            }
            g_object_unref(model);
        } else if (error) {
            std::cerr << "Error get types of columns: " << error->message << std::endl;
            g_error_free(error);
        }
#ifdef USE_LIBGDA6
        gda_connection_close(connection, NULL);
#else
        gda_connection_close(connection);
#endif
        g_object_unref(connection);
        return column_types;
    }
};

void populate_database_with_data(GdaConnection* connection) {
    if (!connection) {
        return;
    }

    GError* execute_error = NULL;

#ifdef USE_POSTGRESQL
    const char* create_table_sql =
        "CREATE TABLE IF NOT EXISTS data_types ("
        "id INTEGER PRIMARY KEY, "
        "name VARCHAR(255), "
        "amount NUMERIC(10, 2), "
        "creation_date TIMESTAMP, "
        "creation_time TIME"
        ")";
#else
    const char* create_table_sql =
        "CREATE TABLE IF NOT EXISTS data_types ("
        "id INTEGER PRIMARY KEY, "
        "name TEXT, "
        "amount REAL, "
        "creation_date TIMESTAMP, "
        "creation_time TIME"
        ")";
#endif

    gda_connection_execute_non_select_command(connection, create_table_sql, &execute_error);
    if (execute_error) {
        std::cerr << "Error create table: " << execute_error->message << std::endl;
        g_error_free(execute_error);
        return;
    }

    GdaDataModel* model = gda_connection_execute_select_command(connection, "SELECT COUNT(*) FROM data_types", &execute_error);
    if (model && gda_data_model_get_n_rows(model) > 0) {
        const GValue* value = gda_data_model_get_value_at(model, 0, 0, NULL);
        if (value && g_value_get_int(value) > 0) {
            std::cout << "Data already exists. Skip inserting." << std::endl;
            g_object_unref(model);
            return;
        }
        g_object_unref(model);
    } else if (execute_error) {
        std::cerr << "Error check count of record: " << execute_error->message << std::endl;
        g_error_free(execute_error);
        return;
    }

    for (int i = 1; i <= 1000; ++i) {
        std::string name = "Item " + std::to_string(i);
        double amount = 100.0 + (i * 0.5);

        int year = 2023;
        int month = (i % 12) + 1;
        int day = (i % 28) + 1;
        int hour = (i % 24);
        int minute = (i % 60);
        int second = (i % 60);

        gchar* insert_cmd = g_strdup_printf(
            "INSERT INTO data_types (id, name, amount, creation_date, creation_time) VALUES(%d, '%s', %f, '%d-%02d-%02d %02d:%02d:%02d', '%02d:%02d:%02d')",
            i,
            name.c_str(),
            amount,
            year, month, day, hour, minute, second,
            hour, minute, second
        );
        gda_connection_execute_non_select_command(connection, insert_cmd, &execute_error);
        g_free(insert_cmd);

        if (execute_error) {
            std::cerr << "Error insert data: " << execute_error->message << std::endl;
            g_error_free(execute_error);
            break;
        }
    }
}

void create(const std::string& provider_name, const std::string& connection_string) {
    gda_init();
    GError* open_error = NULL;

    GdaConnection* connection = gda_connection_open_from_string(
        provider_name.c_str(),
        connection_string.c_str(),
        NULL,
        GDA_CONNECTION_OPTIONS_NONE,
        &open_error
    );

    if (open_error) {
        std::cerr << "Error open/create database: " << open_error->message << std::endl;
        g_error_free(open_error);
    } else {
        populate_database_with_data(connection);
        g_object_unref(connection);
    }
}

class MyWindow : public Gtk::ApplicationWindow {
public:
    MyWindow(DatabaseManager& db_manager)
        : m_db_manager(db_manager), m_current_offset(0), m_page_size(50) {
        set_title("Database viewer with Gtk::TreeView");
        set_default_size(800, 400);

        auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL, 5);

        m_select_columns_button.set_label("Show preselected columns only");
        m_select_columns_button.set_margin(5);
        m_select_columns_button.signal_clicked().connect(
            sigc::mem_fun(*this, &MyWindow::on_select_columns_button_clicked));
        box->append(m_select_columns_button);

        m_sort_by_date_button.set_label("Order by date");
        m_sort_by_date_button.set_margin(5);
        m_sort_by_date_button.signal_clicked().connect(
            sigc::mem_fun(*this, &MyWindow::on_sort_by_date_button_clicked));
        box->append(m_sort_by_date_button);

        m_scrolledwindow.set_child(m_treeview);
        m_scrolledwindow.set_vexpand(true);
        box->append(m_scrolledwindow);
        set_child(*box);

        m_scrolledwindow.signal_edge_reached().connect(
            sigc::mem_fun(*this, &MyWindow::on_edge_reached));

        m_list_store = Gtk::ListStore::create(m_columns);
        m_treeview.set_model(m_list_store);

        load_data_types(true);
    }

protected:
    class ModelColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        ModelColumns() {}

        std::vector<Gtk::TreeModelColumn<Glib::ustring>> m_cols;

        void add_columns(size_t count) {
            m_cols.resize(count);
            for (size_t i = 0; i < count; ++i) {
                add(m_cols[i]);
            }
        }
    };

private:
    DatabaseManager& m_db_manager;

    gint m_current_offset;
    const gint m_page_size;

    Gtk::TreeView m_treeview;
    Gtk::ScrolledWindow m_scrolledwindow;
    Gtk::Button m_select_columns_button;
    Gtk::Button m_sort_by_date_button;

    ModelColumns m_columns;
    Glib::RefPtr<Gtk::ListStore> m_list_store;

    void on_select_columns_button_clicked() {
        m_db_manager.set_column_selection("id, name, amount");
        reset_and_reload_data();
    }

    void on_sort_by_date_button_clicked() {
        m_db_manager.set_column_selection("*");
        m_db_manager.set_order_by("ORDER BY creation_date DESC");
        reset_and_reload_data();
    }

    void reset_and_reload_data() {
        m_list_store->clear();
        for (const auto& column : m_treeview.get_columns()) {
            m_treeview.remove_column(*column);
        }
        m_current_offset = 0;

        load_data_types(true);
    }


    void on_edge_reached(Gtk::PositionType pos) {
        if (pos == Gtk::PositionType::BOTTOM) {
            std::cout << "Reached bottom edge, loading more data..." << std::endl;
            load_data_types(false);
        }
    }

    void load_data_types(bool is_initial_load) {
        std::string table_name = "data_types";

        DatabaseManager::TableData table_data = m_db_manager.get_paged_table_data(table_name, m_page_size, m_current_offset);

        if (table_data.rows_data.empty() && is_initial_load) {
            auto dialog = Gtk::AlertDialog::create("Error database connection or no data.");
            dialog->show(*this);
            return;
        }

        if (table_data.rows_data.empty() && !is_initial_load) {
            std::cout << "No more data to load." << std::endl;
            return;
        }

        if (is_initial_load) {
            m_columns.add_columns(table_data.column_names.size());
            m_list_store = Gtk::ListStore::create(m_columns);
            m_treeview.set_model(m_list_store);

            for (size_t i = 0; i < table_data.column_names.size(); ++i) {
                auto header_box = Gtk::manage(new Gtk::Box(Gtk::Orientation::VERTICAL, 5));
                auto header_entry = Gtk::manage(new Gtk::Entry());
                auto header_label = Gtk::manage(new Gtk::Label(table_data.column_names[i]));
                header_box->append(*header_label);
                header_box->append(*header_entry);

                auto column = Gtk::manage(new Gtk::TreeViewColumn());

                column->set_widget(*header_box);

                auto renderer_text = Gtk::manage(new Gtk::CellRendererText());
                column->pack_start(*renderer_text, true);
                column->add_attribute(*renderer_text, "text", m_columns.m_cols[i]);

                m_treeview.append_column(*column);
            }
        }

        for (const auto& row : table_data.rows_data) {
            Gtk::TreeIter iter = m_list_store->append();
            for (size_t i = 0; i < row.size(); ++i) {
                iter->set_value(m_columns.m_cols[i], Glib::ustring(row[i]));
            }
        }

        m_current_offset += table_data.rows_data.size();
    }
};


int main(int argc, char* argv[]) {
#ifdef USE_POSTGRESQL
    std::string provider = "PostgreSQL";
    std::string connection_string = "host=localhost;port=5432;user=my_user;password=my_pass;dbname=databaze_name";
#else
    gchar* current_dir = g_get_current_dir();
    std::string sqlite_conn_string = "DB_DIR=" + std::string(current_dir) + ";DB_NAME=users";
    g_free(current_dir);

    std::string provider = "SQLite";
    std::string connection_string = sqlite_conn_string;
#endif

    create(provider, connection_string);
    DatabaseManager db_manager(provider, connection_string);

    db_manager.set_order_by("ORDER BY id");

    std::cout << "----- Information about database scheme -----" << std::endl;
    std::string table_name = "data_types";

    std::vector<std::string> column_names = db_manager.get_cached_column_names();
    std::vector<std::string> column_types = db_manager.get_cached_column_types();

    if (column_names.size() == column_types.size()) {
        for (size_t i = 0; i < column_names.size(); ++i) {
            std::cout << "Column name: " << column_names[i] << " | Type: " << column_types[i] << std::endl;
        }
    } else {
        std::cerr << "Error: Count of column names and column types not equal." << std::endl;
    }
    std::cout << "--------------------------------" << std::endl;

    auto app = Gtk::Application::create("org.gtkmm.example");

    app->signal_activate().connect([app, &db_manager]() {
        auto window = Gtk::make_managed<MyWindow>(db_manager);
        app->add_window(*window);
        window->present();
    });

    return app->run(argc, argv);
}
