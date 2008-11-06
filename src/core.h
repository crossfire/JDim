// ライセンス: GPL2

//
// コアクラス
//

#ifndef _CORE_H
#define _CORE_H

#include <gtkmm.h>
#include <list>
#include <string>

#include "skeleton/dispatchable.h"
#include "skeleton/imgbutton.h"
#include "skeleton/hpaned.h"
#include "skeleton/vpaned.h"
#include "skeleton/notebook.h"
#include "skeleton/vbox.h"

#include "command_args.h"

class WinMain;


namespace BOARD
{
    class BoardAdmin;
}


namespace BBSLIST
{
    class BBSListAdmin;
}


namespace ARTICLE
{
    class ArticleAdmin;
}

namespace IMAGE
{
    class ImageAdmin;
}


namespace MESSAGE
{
    class MessageAdmin;
}

namespace CORE
{
    class MainToolBar;
    class DND_Manager;

    class Core : public SKELETON::Dispatchable
    {
        std::list< COMMAND_ARGS > m_list_command;
        sigc::connection m_sigc_switch_page;

        WinMain& m_win_main;

        SKELETON::JDHPaned m_hpaned;

        // サイドバー
        Gtk::Widget* m_sidebar;

        // (縦/横) 3ペーンモード時の右側ペーン
        SKELETON::JDVPaned m_vpaned_r; 
        SKELETON::JDHPaned m_hpaned_r;

        // 右ペーンで使用するwidget
        SKELETON::JDVBox m_vbox_article;
        SKELETON::JDVBox m_vbox_toolbar;
        SKELETON::JDNotebook m_notebook_right;
        bool m_imagetab_shown;
        SKELETON::JDVPaned m_vpaned_message; // 埋め込み書き込みビュー用

        // ツールバー
        MainToolBar* m_toolbar;

        // タイトルに表示する文字列
        // set_maintitle() 参照
        std::string m_title;

        Gtk::MenuBar* m_menubar;

        Glib::RefPtr< Gtk::ActionGroup > m_action_group;
        Glib::RefPtr< Gtk::UIManager > m_ui_manager;
        bool m_enable_menuslot;

        // 初期設定中
        bool m_init;

    public:

        Core( WinMain& win_main );
        virtual ~Core();

        Gtk::Widget* get_toplevel();

        // init = true なら初回起動
        void run( bool init );        

        void set_command( const COMMAND_ARGS& command );

        // SIGHUPを受け取った時の処理
        void shutdown();

    private:

        bool is_3pane();
        bool is_all_admin_empty();

        Gtk::Paned* get_rpane();
        SKELETON::PaneControl* get_rpctrl();

        void pack_widget( bool unpack );
        void create_toolbar();

        // 初回起動時のセットアップ
        void first_setup();

        void set_maintitle();

        void slot_realize();
        void slot_style_changed( Glib::RefPtr< Gtk::Style > );

        void slot_activate_menubar();

        void slot_toggle_save_postlog();
        void slot_toggle_use_mosaic();
        void slot_toggle_use_imgpopup();
        void slot_toggle_use_inlineimg();
        void slot_toggle_show_ssspicon();
        void slot_delete_all_images();

        void slot_clear_privacy();
        void slot_clear_board();
        void slot_clear_thread();
        void slot_clear_close();
        void slot_clear_search();
        void slot_clear_name();
        void slot_clear_mail();
        void slot_clear_write_log();
        void slot_clear_post_log();

        void slot_changefont_main();
        void slot_changefont_popup();
        void slot_changefont_tree();
        void slot_changecolor_char();
        void slot_changecolor_back();
        void slot_changecolor_char_tree();
        void slot_changecolor_back_tree();
        bool open_color_diag( std::string title, int id );
        
        void slot_setup_fontcolor();
        void slot_setup_key();
        void slot_setup_mouse();
        void slot_setup_button();
        void slot_setup_proxy();
        void slot_setup_passwd();
        void slot_toggle_ipv6();
        void slot_setup_mainitem();
        void slot_setup_sidebaritem();
        void slot_setup_boarditemcolumn();
        void slot_setup_boarditem();
        void slot_setup_articleitem();
        void slot_setup_msgitem();
        void slot_board_pref();
        void slot_article_pref();
        void slot_image_pref();
        void slot_usrcmd_pref();
        void slot_filter_pref();
        void slot_setup_browser();
        void slot_setup_abone();
        void slot_setup_abone_thread();
        void slot_toggle_abone_transp_chain();
        void slot_setup_live();

        void slot_show_bbs();
        void slot_show_old2ch();
        void slot_show_manual();
        void slot_aboutconfig();
        void slot_show_about();
        void slot_quit();
        void slot_reload_list();
        void slot_check_update_root();
        void slot_check_update_open_root();
        void slot_cancel_check_update();
        void slot_save_favorite();
        void slot_toggle_online();
        void slot_toggle_login2ch();
        void slot_toggle_loginbe();
        void slot_toggle_loginp2();
        void toggle_menubar();
        void toggle_flat_button();
        void toggle_post_mark();
        void toggle_sidebar();
        void slot_search_cache_board();
        void slot_search_cache();
        void slot_search_title();
        void slot_show_postlog();
        void slot_live_start_stop();
        void slot_show_hide_leftpane( int mode );
        void slot_toggle_toolbarpos( int pos );
        void slot_toggle_toolbarbbslist();
        void slot_toggle_toolbararticle();
        void slot_toggle_toolbarboard();
        void slot_toggle_tabboard();
        void slot_toggle_tabarticle();
        void slot_toggle_2pane();
        void slot_toggle_3pane();
        void slot_toggle_v3pane();
        void slot_toggle_winmsg();
        void slot_toggle_embmsg();
        void slot_toggle_imgview( const int mode );
        void slot_toggle_oldarticle();
        void slot_toggle_tabbutton();
        void slot_toggle_popupwarpmode();
        void slot_shortmargin_popup();
        void slot_toggle_emacsmode();
        void slot_toggle_restore_views();
        void slot_toggle_fold_message();

        virtual void callback_dispatch();

        // coreが自前でするコマンド処理
        void exec_command();

        // 起動完了直後に実行する処理
        void exec_command_after_boot();

        // フォーカス回復
        void restore_focus( const bool force, const bool present );

        // メインタイマー
        bool slot_timeout( int timer_number );

        // 右ペーンのnotebookのタブの切替え
        void slot_switch_page( GtkNotebookPage*, guint page );

        // 右ペーンのnotebookのページ番号
        int get_right_current_page();

        // 右ペーンのnotebookのページをセット
        void set_right_current_page( int page );

        bool slot_focus_out_event( GdkEventFocus* ev );
        bool slot_focus_in_event( GdkEventFocus* ev );

        // URL entryでenterを押した
        void slot_active_url();

        // あるadminがemptyになったので他のadminにスイッチ
        void empty_page( const std::string& url );

        void set_toggle_view_button();
        void set_sensitive_view_button();
        void toggle_maximize_rightpane();

        void switch_article( bool present );
        void switch_board( bool present );
        void switch_sidebar( const std::string& url, bool present );
        void switch_image( bool present );
        void switch_message( bool present );

        void toggle_article();
        void switch_leftview();
        void switch_rightview();
        void open_by_browser( const std::string& url );

        void set_history_article( const std::string& url );
        void set_history_board( const std::string& url );
        void set_history_close( const std::string& url );

        // 画像インジケータ表示/非表示
        void show_imagetab();
        void hide_imagetab();
    };

    Core* get_instance();
}


#endif
