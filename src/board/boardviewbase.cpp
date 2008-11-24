// ライセンス: GPL2

//#define _DEBUG
#include "jddebug.h"

#include "boardadmin.h"
#include "boardview.h"

#include "skeleton/msgdiag.h"

#include "jdlib/miscutil.h"
#include "jdlib/miscgtk.h"
#include "jdlib/jdregex.h"

#include "dbtree/interface.h"
#include "dbtree/articlebase.h"

#include "config/globalconf.h"

#include "control/controlid.h"
#include "control/controlutil.h"

#include "command.h"
#include "session.h"
#include "dndmanager.h"
#include "sharedbuffer.h"
#include "global.h"
#include "type.h"
#include "prefdiagfactory.h"
#include "httpcode.h"
#include "colorid.h"
#include "fontid.h"
#include "compmanager.h"

#include "icons/iconmanager.h"

#include <gtk/gtk.h> // m_liststore->gobj()->sort_column_id = -2
#include <sstream>

using namespace BOARD;


// row -> path
#define GET_PATH( row ) m_liststore->get_path( row )



// 自動ソート抑制
// -2 = DEFAULT_UNSORTED_COLUMN_ID
//
// 列に値をセットする前にUNSORTED_COLUMN()しておかないと
// いちいちソートをかけるので極端に遅くなる
#define UNSORTED_COLUMN() do{ m_liststore->gobj()->sort_column_id = -2; } while(0)


enum{
    DEFAULT_COLMUN_WIDTH = 50
};


enum{
    COL_MARKVAL_OLD = -2,        // dat 落ち
    COL_MARKVAL_FINISHED = -1,   // キャッシュあり、新着無し、規定スレ数を越えている
    COL_MARKVAL_NORMAL = 0,      // 通常状態
    COL_MARKVAL_NEWTHREAD_HOUR,  // 新スレ( CONFIG::get_newthread_hour 時間以内 )
    COL_MARKVAL_NEWTHREAD,       // 前回の板一覧読み込み時から新しく出来たスレ
    COL_MARKVAL_CACHED,          // キャッシュあり、新着無し
    COL_MARKVAL_UPDATED,         // キャッシュあり、新着有り
    COL_MARKVAL_BKMARKED,        // ブックマークされている、新着無し
    COL_MARKVAL_BKMARKED_UPDATED // ブックマークされている、新着有り
};


// 昇順で上か下か
enum{
    COL_A_UP = -1,
    COL_B_UP = 1,
};


enum
{
    BOOKMARK_AUTO = -1,
    BOOKMARK_UNSET = 0,
    BOOKMARK_SET = 1
};


// set_sizing( Gtk::TREE_VIEW_COLUMN_FIXED ) を指定して append_columnする
#define APPEND_COLUMN(col,title,model) do{                    \
if( ! col ) delete col; \
col = Gtk::manage( new Gtk::TreeViewColumn( title, model ) );   \
col->set_sizing( Gtk::TREE_VIEW_COLUMN_FIXED ); \
m_treeview.append_column( *col ); \
}while(0)


BoardViewBase::BoardViewBase( const std::string& url )
    : SKELETON::View( url ),
      m_url_board( url ),
      m_treeview( true, CONFIG::get_fontname( FONT_BOARD ), COLOR_CHAR_BOARD, COLOR_BACK_BOARD, COLOR_BACK_BOARD_EVEN ),
      m_col_mark( NULL ),
      m_col_id( NULL ),
      m_col_subject( NULL ),
      m_col_res( NULL ),
      m_col_str_load( NULL ),     
      m_col_str_new( NULL ),
      m_col_since( NULL ),
      m_col_write( NULL ),
      m_col_speed( NULL ),
      m_col( COL_NUM_COL ),
      m_previous_col( COL_NUM_COL ),
      m_sortmode( SORTMODE_ASCEND ),
      m_previous_sortmode( false ),
      m_updated( false ),
      m_loading( false )
{
    m_scrwin.add( m_treeview );
    m_scrwin.set_policy( Gtk::POLICY_AUTOMATIC, Gtk::POLICY_ALWAYS );

    pack_start( m_scrwin );
    show_all_children();    

    // ツリービュー設定
    m_liststore = Gtk::ListStore::create( m_columns );

#if GTKMMVER <= 260
    // gtkmm26以下にはunset_model()が無いのでここでset_model()しておく
    m_treeview.set_model( m_liststore );
#endif

#if GTKMMVER >= 260

    // セルを固定の高さにする
    // append_column する前に columnに対して set_sizing( Gtk::TREE_VIEW_COLUMN_FIXED ) すること
    m_treeview.set_fixed_height_mode( true );

#ifdef _DEBUG
    std::cout << "BoardViewBase::BoardViewBase : m_treeview.set_fixed_height_mode\n";
#endif

#endif

    // 列のappend
    update_columns();

    // ソート関数セット
    m_liststore->set_sort_func( COL_MARK, sigc::mem_fun( *this, &BoardViewBase::slot_compare_row ) );    
    m_liststore->set_sort_func( COL_ID, sigc::mem_fun( *this, &BoardViewBase::slot_compare_row ) );
    m_liststore->set_sort_func( COL_SUBJECT, sigc::mem_fun( *this, &BoardViewBase::slot_compare_row ) );
    m_liststore->set_sort_func( COL_RES, sigc::mem_fun( *this, &BoardViewBase::slot_compare_row ) );
    m_liststore->set_sort_func( COL_STR_LOAD, sigc::mem_fun( *this, &BoardViewBase::slot_compare_row ) );
    m_liststore->set_sort_func( COL_STR_NEW, sigc::mem_fun( *this, &BoardViewBase::slot_compare_row ) );
    m_liststore->set_sort_func( COL_SINCE, sigc::mem_fun( *this, &BoardViewBase::slot_compare_row ) );
    m_liststore->set_sort_func( COL_WRITE, sigc::mem_fun( *this, &BoardViewBase::slot_compare_row ) );
    m_liststore->set_sort_func( COL_SPEED, sigc::mem_fun( *this, &BoardViewBase::slot_compare_row ) );
    
    m_treeview.sig_button_press().connect( sigc::mem_fun(*this, &BoardViewBase::slot_button_press ) );
    m_treeview.sig_button_release().connect( sigc::mem_fun(*this, &BoardViewBase::slot_button_release ) );
    m_treeview.sig_motion_notify().connect( sigc::mem_fun(*this, &BoardViewBase::slot_motion_notify ) );
    m_treeview.sig_key_press().connect( sigc::mem_fun(*this, &BoardViewBase::slot_key_press ) );
    m_treeview.sig_key_release().connect( sigc::mem_fun(*this, &BoardViewBase::slot_key_release ) );
    m_treeview.sig_scroll_event().connect( sigc::mem_fun(*this, &BoardViewBase::slot_scroll_event ) );

    // D&D設定
    m_treeview.sig_drag_begin().connect( sigc::mem_fun(*this, &BoardViewBase::slot_drag_begin ) );
    m_treeview.sig_drag_end().connect( sigc::mem_fun(*this, &BoardViewBase::slot_drag_end ) );


    // ポップアップメニューの設定
    // アクショングループを作ってUIマネージャに登録
    action_group() = Gtk::ActionGroup::create();
    action_group()->add( Gtk::Action::create( "BookMark", "しおりを設定/解除(_B)" ),
                         sigc::bind< int >( sigc::mem_fun( *this, &BoardViewBase::slot_bookmark ), BOOKMARK_AUTO ) );
    action_group()->add( Gtk::Action::create( "SetBookMark", "しおりを設定(_S)" ),  // 未使用
                         sigc::bind< int >( sigc::mem_fun( *this, &BoardViewBase::slot_bookmark ), BOOKMARK_SET ) );
    action_group()->add( Gtk::Action::create( "UnsetBookMark", "しおりを解除(_U)" ),    // 未使用
                         sigc::bind< int >( sigc::mem_fun( *this, &BoardViewBase::slot_bookmark ), BOOKMARK_UNSET ) );
    action_group()->add( Gtk::Action::create( "OpenTab", "OpenArticleTab" ), sigc::mem_fun( *this, &BoardViewBase::slot_open_tab ) );
    action_group()->add( Gtk::Action::create( "Favorite_Article", "スレをお気に入りに追加(_F)..." ), sigc::mem_fun( *this, &BoardViewBase::slot_favorite_thread ) );
    action_group()->add( Gtk::Action::create( "Favorite_Board", "板をお気に入りに登録(_A)" ), sigc::mem_fun( *this, &BoardViewBase::slot_favorite_board ) );
    action_group()->add( Gtk::Action::create( "GotoTop", "一番上に移動(_T)" ), sigc::mem_fun( *this, &BoardViewBase::goto_top ) );
    action_group()->add( Gtk::Action::create( "GotoBottom", "一番下に移動(_M)" ), sigc::mem_fun( *this, &BoardViewBase::goto_bottom ) );
    action_group()->add( Gtk::Action::create( "Delete_Menu", "Delete" ) );
    action_group()->add( Gtk::Action::create( "Delete", "選択した行のログを削除する(_D)" ), sigc::mem_fun( *this, &BoardViewBase::slot_delete_logs ) );
    action_group()->add( Gtk::Action::create( "OpenRows", "選択した行を開く(_O)" ), sigc::mem_fun( *this, &BoardViewBase::open_selected_rows ) );
    action_group()->add( Gtk::Action::create( "CopyURL", "URLをコピー(_C)" ), sigc::mem_fun( *this, &BoardViewBase::slot_copy_url ) );
    action_group()->add( Gtk::Action::create( "CopyTitleURL", "タイトルとURLをコピー(_U)" ), sigc::mem_fun( *this, &BoardViewBase::slot_copy_title_url ) );
    action_group()->add( Gtk::Action::create( "OpenBrowser", "ブラウザで開く(_W)" ), sigc::mem_fun( *this, &BoardViewBase::slot_open_browser ) );
    action_group()->add( Gtk::Action::create( "AboneThread", "スレをあぼ〜んする(_N)" ), sigc::mem_fun( *this, &BoardViewBase::slot_abone_thread ) );
    action_group()->add( Gtk::Action::create( "PreferenceArticle", "スレのプロパティ(_I)..." ), sigc::mem_fun( *this, &BoardViewBase::slot_preferences_article ) );
    action_group()->add( Gtk::Action::create( "Preference", "板のプロパティ(_P)..." ), sigc::mem_fun( *this, &BoardViewBase::show_preference ) );
    action_group()->add( Gtk::Action::create( "SaveDat", "datファイルを保存(_S)..." ), sigc::mem_fun( *this, &BoardViewBase::slot_save_dat ) );

    ui_manager() = Gtk::UIManager::create();    
    ui_manager()->insert_action_group( action_group() );

    Glib::ustring str_ui = 
    "<ui>"

    // 通常
    "<popup name='popup_menu'>"
    "<menuitem action='BookMark'/>"
    "<separator/>"
    "<menuitem action='OpenTab'/>"
    "<menuitem action='OpenBrowser'/>"
    "<separator/>"
    "<menuitem action='CopyURL'/>"
    "<menuitem action='CopyTitleURL'/>"
    "<separator/>"
    "<menuitem action='SaveDat'/>"
    "<separator/>"
    "<menuitem action='Favorite_Article'/>"
    "<separator/>"
    "<menuitem action='AboneThread'/>"
    "<separator/>"
    "<menu action='Delete_Menu'>"
    "<menuitem action='Delete'/>"
    "</menu>"
    "<separator/>"
    "<menuitem action='PreferenceArticle'/>"
    "<menuitem action='Preference'/>"
    "</popup>"


    // 通常 + 複数
    "<popup name='popup_menu_mul'>"
    "<menuitem action='OpenRows'/>"
    "<separator/>"
    "<menuitem action='SetBookMark'/>"
    "<menuitem action='UnsetBookMark'/>"
    "<separator/>"
    "<menuitem action='Favorite_Article'/>"
    "<separator/>"
    "<menuitem action='AboneThread'/>"
    "<separator/>"
    "<menu action='Delete_Menu'>"
    "<menuitem action='Delete'/>"
    "</menu>"
    "</popup>"


    // お気に入りボタン押した時のメニュー
    "<popup name='popup_menu_favorite'>"
    "<menuitem action='Favorite_Article'/>"
    "<menuitem action='Favorite_Board'/>"
    "</popup>"


    // 削除ボタン押した時のメニュー
    "<popup name='popup_menu_delete'>"
    "<menuitem action='Delete'/>"
    "</popup>"


    "</ui>";

    ui_manager()->add_ui_from_string( str_ui );

    // ポップアップメニューにキーアクセレータを表示
    Gtk::Menu* popupmenu = dynamic_cast< Gtk::Menu* >( ui_manager()->get_widget( "/popup_menu" ) );
    CONTROL::set_menu_motion( popupmenu );

    popupmenu = dynamic_cast< Gtk::Menu* >( ui_manager()->get_widget( "/popup_menu_mul" ) );
    CONTROL::set_menu_motion( popupmenu );

    // マウスジェスチォ可能
    set_enable_mg( true );

    // コントロールモード設定
    get_control().add_mode( CONTROL::MODE_BOARD );
}


BoardViewBase::~BoardViewBase()
{
#ifdef _DEBUG    
    std::cout << "BoardViewBase::~BoardViewBase : " << get_url() << std::endl;
#endif
    DBTREE::board_save_info( get_url_board() );
    save_column_width();
}


SKELETON::Admin* BoardViewBase::get_admin()
{
    return BOARD::get_admin();
}


// アイコンのID取得
const int BoardViewBase::get_icon( const std::string& iconname )
{
    int id = ICON::NONE;

    if( iconname == "default" ) id = ICON::BOARD;
    if( iconname == "loading" ) id = ICON::LOADING;
    if( iconname == "loading_stop" ) id = ICON::LOADING_STOP;
    if( iconname == "updated" ) id = ICON::BOARD_UPDATED;

    return id;
}


// 更新した
const bool BoardViewBase::is_updated()
{
    const int code = DBTREE::board_code( get_url_board() );
    return ( ( code == HTTP_OK || code == HTTP_PARTIAL_CONTENT ) && m_updated );
}


//
// コピー用URL(メインウィンドウのURLバーなどに表示する)
//
const std::string BoardViewBase::url_for_copy()
{
    return DBTREE::url_boardbase( get_url_board() );
}



//
// 列項目の更新
//
void BoardViewBase::update_columns()
{
    m_treeview.remove_all_columns();

    int num = 0;
    for(;;){
        const int item = SESSION::get_item_board( num );
        if( item == ITEM_END ) break;
        switch( item ){
            case ITEM_MARK: APPEND_COLUMN( m_col_mark, ITEM_NAME_MARK, m_columns.m_col_mark ); break;
            case ITEM_ID: APPEND_COLUMN( m_col_id, ITEM_NAME_ID, m_columns.m_col_id ); break;
            case ITEM_NAME: APPEND_COLUMN( m_col_subject, ITEM_NAME_NAME, m_columns.m_col_subject ); break;
            case ITEM_RES: APPEND_COLUMN( m_col_res, ITEM_NAME_RES, m_columns.m_col_res ); break;
            case ITEM_LOAD: APPEND_COLUMN( m_col_str_load, ITEM_NAME_LOAD, m_columns.m_col_str_load ); break;
            case ITEM_NEW: APPEND_COLUMN( m_col_str_new, ITEM_NAME_NEW, m_columns.m_col_str_new ); break;
            case ITEM_SINCE: APPEND_COLUMN( m_col_since, ITEM_NAME_SINCE, m_columns.m_col_since ); break;
            case ITEM_LASTWRITE: APPEND_COLUMN( m_col_write, ITEM_NAME_LASTWRITE, m_columns.m_col_write ); break;
            case ITEM_SPEED: APPEND_COLUMN( m_col_speed, ITEM_NAME_SPEED, m_columns.m_col_speed ); break;
        }
        ++num;
    }

    // サイズを調整しつつソートの設定
    for( guint i = 0; i < COL_VISIBLE_END; i++ ){
        
        const int id = get_title_id( i );
        if( id < 0 ) continue;

        Gtk::TreeView::Column* column = m_treeview.get_column( i );
        if( ! column ) continue;

        int width = 0;

        switch( id ){

        case COL_MARK:
            width = SESSION::col_mark();
            break;

        case COL_ID:
            width = SESSION::col_id();
            break;

        case COL_SUBJECT:
            width = SESSION::col_subject();
            m_treeview.set_column_for_height( id );
            break;

        case COL_RES:
            width = SESSION::col_number();
            break;

        case COL_STR_LOAD:
            width = SESSION::col_load();
            break;

        case COL_STR_NEW:
            width = SESSION::col_new();
            break;

        case COL_SINCE:
            width = SESSION::col_since();
            break;

        case COL_WRITE:
            width = SESSION::col_write();
            break;

        case COL_SPEED:
            width = SESSION::col_speed(); 
            break;
        }

        if( ! width ) width = DEFAULT_COLMUN_WIDTH;

        column->set_fixed_width( width );
        column->set_resizable( true );
        column->set_clickable( true );        

        // ヘッダをクリックしたときに呼ぶslot
        column->signal_clicked().connect( sigc::bind< int >( sigc::mem_fun( *this, &BoardViewBase::slot_col_clicked ), id ) );

        // ヘッダの位置
        switch( id ){
            case COL_MARK:
                column->set_alignment( 0.5 );
                break;

            case COL_ID:
            case COL_RES:
            case COL_STR_LOAD:
            case COL_STR_NEW:
            case COL_SPEED:
                column->set_alignment( 1.0 );
                break;

            default:
                column->set_alignment( 0.0 );
                break;
        }

        Gtk::CellRenderer *cell = column->get_first_cell_renderer();

        // 実際の描画の際に cellrendere のプロパティをセットするスロット関数
        if( cell ) column->set_cell_data_func( *cell, sigc::mem_fun( *this, &BoardViewBase::slot_cell_data ) );

        Gtk::CellRendererText* rentext = dynamic_cast< Gtk::CellRendererText* >( cell );
        if( rentext ){

            // 列間スペース
            rentext->property_xpad() = 4;

            // 行間スペース
            rentext->property_ypad() = CONFIG::get_tree_ypad();;

            // 文字位置
            switch( id ){
                case COL_ID:
                case COL_RES:
                case COL_STR_LOAD:
                case COL_STR_NEW:
                case COL_SPEED:
                    rentext->property_xalign() = 1.0;
                    break;

                default:
                    rentext->property_xalign() = 0.0;
            }
        }
    }
}


//
// i列目のIDを取得
//
// 失敗の時は-1を変えす
//
const int BoardViewBase::get_title_id( const int col )
{
    Gtk::TreeView::Column* column = m_treeview.get_column( col );
    if( ! column ) return -1;

    const std::string title = column->get_title();
    int id = -1;

    if( title == ITEM_NAME_MARK ) id = COL_MARK;
    else if( title == ITEM_NAME_ID ) id = COL_ID;
    else if( title == ITEM_NAME_NAME ) id = COL_SUBJECT;
    else if( title == ITEM_NAME_RES ) id = COL_RES;
    else if( title == ITEM_NAME_LOAD ) id = COL_STR_LOAD;
    else if( title == ITEM_NAME_NEW ) id = COL_STR_NEW;
    else if( title == ITEM_NAME_SINCE ) id = COL_SINCE;
    else if( title == ITEM_NAME_LASTWRITE ) id = COL_WRITE;
    else if( title == ITEM_NAME_SPEED ) id = COL_SPEED;

    return id;
}



//
// ソート列やソートモードの保存
//
void BoardViewBase::save_sort_columns()
{
#ifdef _DEBUG
    std::cout << "BoardViewBase::save_sort_columns : url = " << get_url() << std::endl;
#endif

    DBTREE::board_set_view_sort_column( get_url_board(), m_col );
    DBTREE::board_set_view_sort_mode( get_url_board(), m_sortmode );
    DBTREE::board_set_view_sort_pre_column( get_url_board(), m_previous_col );
    DBTREE::board_set_view_sort_pre_mode( get_url_board(), m_previous_sortmode );
}


//
// 列の幅の保存
//
void BoardViewBase::save_column_width()
{
#ifdef _DEBUG
    std::cout << "save_column_width " << get_url() << std::endl;
#endif    

    for( guint i = 0; i < COL_VISIBLE_END; i++ ){
        
        const int id = get_title_id( i );
        if( id < 0 ) continue;

        Gtk::TreeView::Column* column = m_treeview.get_column( i );

        int width = 0;
        if( column ) width = column->get_width();
        if( ! width ) continue;

        switch( id ){

        case COL_MARK:
            SESSION::set_col_mark( width );
            break;

        case COL_ID:
            SESSION::set_col_id( width );
            break;

        case COL_SUBJECT:
            SESSION::set_col_subject( width );
            break;

        case COL_RES:
            SESSION::set_col_number( width );
            break;

        case COL_STR_LOAD:
            SESSION::set_col_load( width );
            break;

        case COL_STR_NEW:
            SESSION::set_col_new( width );
            break;

        case COL_SINCE:
            SESSION::set_col_since( width );
            break;

        case COL_WRITE:
            SESSION::set_col_write( width );
            break;

        case COL_SPEED:
            SESSION::set_col_speed( width );
            break;
        }
    }
} 
   


//
// 実際の描画の際に cellrendere のプロパティをセットするスロット関数
//
void BoardViewBase::slot_cell_data( Gtk::CellRenderer* cell, const Gtk::TreeModel::iterator& it )
{
    Gtk::TreeModel::Row row = *it;
    Gtk::TreePath path = m_liststore->get_path( row );

#ifdef _DEBUG
//    std::cout << "BoardViewBase::slot_cell_data path = " << path.to_string() << std::endl;
#endif

    // ハイライト色 ( 抽出状態 )
    if( row[ m_columns.m_col_drawbg ] ){
        cell->property_cell_background() = CONFIG::get_color( COLOR_BACK_HIGHLIGHT_TREE );
        cell->property_cell_background_set() = true;
    }

    else m_treeview.slot_cell_data( cell, it );
}



//
// ソート実行
//
void BoardViewBase::exec_sort()
{
#ifdef _DEBUG
    std::cout << "BoardViewBase::exec_sort url = " << get_url() << std::endl;
#endif

    if( m_col < 0 || m_col >= COL_VISIBLE_END ){
        m_col = COL_ID;
        m_sortmode = SORTMODE_ASCEND;
    }

    m_liststore->set_sort_column( -2, Gtk::SORT_ASCENDING );
    m_liststore->set_sort_column( m_col, Gtk::SORT_ASCENDING );
}


//
// ソート状態回復
//
void BoardViewBase::restore_sort()
{
    m_search_invert = false;

    m_col = DBTREE::board_view_sort_column( get_url_board() );
    m_sortmode = DBTREE::board_view_sort_mode( get_url_board() );

    m_previous_col = DBTREE::board_view_sort_pre_column( get_url_board() );
    m_previous_sortmode = DBTREE::board_view_sort_pre_mode( get_url_board() );

    Gtk::TreeModel::Children child = m_liststore->children();
    if( child.size() ){
        exec_sort();
        goto_top();
    }
}


//
// ヘッダをクリックしたときのslot関数
//
void BoardViewBase::slot_col_clicked( const int col )
{
#ifdef _DEBUG
    std::cout << "BoardViewBase::slot_col_clicked col = " << col << std::endl;
#endif

    if( m_col != col ){ // 前回クリックした列と違う列をクリックした

        m_previous_col = m_col;
        m_previous_sortmode = m_sortmode;

        m_col = col;

        if( m_col == COL_MARK ) m_sortmode = SORTMODE_MARK1;
        else if( m_col == COL_ID ) m_sortmode = SORTMODE_ASCEND;
        else if( m_col == COL_SUBJECT ) m_sortmode = SORTMODE_ASCEND;
        else m_sortmode = SORTMODE_DESCEND;
    }
    else if( m_sortmode == SORTMODE_DESCEND ) m_sortmode = SORTMODE_ASCEND;
    else if( m_sortmode == SORTMODE_ASCEND ) m_sortmode = SORTMODE_DESCEND;
    else if( m_sortmode == SORTMODE_MARK1 ) m_sortmode = SORTMODE_MARK2;
    else if( m_sortmode == SORTMODE_MARK2 ) m_sortmode = SORTMODE_MARK3;
    else if( m_sortmode == SORTMODE_MARK3 ) m_sortmode = SORTMODE_MARK1;

    // 旧バージョンとの互換性のため
    if( m_col == COL_MARK ){
        if( m_sortmode == SORTMODE_DESCEND || m_sortmode == SORTMODE_ASCEND ) m_sortmode = SORTMODE_MARK1;
    }

    save_sort_columns();
    exec_sort();
    focus_view();
}


//
// 抽出状態で比較
//
// row_a が上か　row_b　が上かを返す。同じ状態なら 0
//
const int BoardViewBase::compare_drawbg( Gtk::TreeModel::Row& row_a, Gtk::TreeModel::Row& row_b )
{
    const bool draw_a = row_a[ m_columns.m_col_drawbg ];
    const bool draw_b = row_b[ m_columns.m_col_drawbg ];

    if( draw_a && ! draw_b ) return COL_A_UP;
    else if( draw_b && ! draw_a ) return COL_B_UP;

    return 0;
}


//
// 列の値によるソート
//
// row_a が上か　row_b　が上かを返す。同じなら 0
//
const int BoardViewBase::compare_col( const int col, const int sortmode, Gtk::TreeModel::Row& row_a, Gtk::TreeModel::Row& row_b )
{
    int num_a = 0, num_b = 0;
    int ret = 0;

    const int UP = 1;
    const int DOWN = 2;

    switch( col ){

        case COL_MARK:
        {
            num_a = row_a[ m_columns.m_col_mark_val ];
            num_b = row_b[ m_columns.m_col_mark_val ];

            if( sortmode == SORTMODE_MARK2 ){ // 新着をキャッシュの上に

                if( num_a == COL_MARKVAL_NEWTHREAD
                    && ( num_b != COL_MARKVAL_NEWTHREAD && num_b != COL_MARKVAL_BKMARKED_UPDATED && num_b != COL_MARKVAL_BKMARKED ) ){
                    num_a = DOWN; // 下で ret *= -1 しているので UP と DOWNを逆にする
                    num_b = UP;
                }
                else if( num_b == COL_MARKVAL_NEWTHREAD
                         && ( num_a != COL_MARKVAL_NEWTHREAD && num_a != COL_MARKVAL_BKMARKED_UPDATED && num_a != COL_MARKVAL_BKMARKED ) ){
                    num_a = UP; // 下で ret *= -1 しているので UP と DOWNを逆にする
                    num_b = DOWN;
                }
            }

            break;
        }

        case COL_ID:
            num_a = row_a[ m_columns.m_col_id ];
            num_b = row_b[ m_columns.m_col_id ];
            break;

        case COL_SUBJECT:
        {
            // 文字列の大小を数字に変換
            Glib::ustring name_a = row_a[ m_columns.m_col_subject ]; 
            Glib::ustring name_b = row_b[ m_columns.m_col_subject ]; 
            if( name_a < name_b ) { num_a = UP; num_b = DOWN; }
            else if( name_a > name_b ) { num_a = DOWN; num_b = UP; }
            break;
        }

        case COL_RES:
            num_a = row_a[ m_columns.m_col_res ];
            num_b = row_b[ m_columns.m_col_res ];
            break;

        case COL_STR_LOAD:
            num_a = row_a[ m_columns.m_col_load ];
            num_b = row_b[ m_columns.m_col_load ];
            break;

        case COL_STR_NEW:
            num_a = row_a[ m_columns.m_col_new ];
            num_b = row_b[ m_columns.m_col_new ];
            break;

        case COL_SINCE:
            num_a = row_a[ m_columns.m_col_since_t ];
            num_b = row_b[ m_columns.m_col_since_t ];
            break;

        case COL_WRITE:
            num_a = row_a[ m_columns.m_col_write_t ];
            num_b = row_b[ m_columns.m_col_write_t ];
            break;

        case COL_SPEED:
            num_a = row_a[ m_columns.m_col_speed ];
            num_b = row_b[ m_columns.m_col_speed ];
            break;
    }

    // 両方とも 0 より大きいか 0 より小さい場合は普通に比較
    if( ( num_a > 0 && num_b > 0 ) || ( num_a < 0 && num_b < 0 ) ){

        if( num_a < num_b ) ret = COL_A_UP;
        else if( num_a > num_b ) ret = COL_B_UP;

        if( sortmode == SORTMODE_DESCEND ) ret *= -1;
        if( sortmode == SORTMODE_MARK1 || sortmode == SORTMODE_MARK2 ) ret *= -1;
    }

    // 0より大きい方を優先
    else if( num_a > 0 && num_b <= 0 ) ret = COL_A_UP;
    else if( num_b > 0 && num_a <= 0 ) ret = COL_B_UP;

    // 0を優先
    else if( num_a == 0 && num_b < 0 ) ret = COL_A_UP;
    else if( num_b == 0 && num_a < 0 ) ret = COL_B_UP;

    return ret;
}


//
// ソート関数
//
const int BoardViewBase::slot_compare_row( const Gtk::TreeModel::iterator& a, const Gtk::TreeModel::iterator& b )
{
    Gtk::TreeModel::Row row_a = *( a );
    Gtk::TreeModel::Row row_b = *( b );

    // 抽出状態を最優先
    int ret = compare_drawbg( row_a, row_b ); 

    if( ! ret ) ret = compare_col( m_col, m_sortmode, row_a, row_b );

    // マルチキーソート
    if( ! ret ) ret = compare_col( m_previous_col, m_previous_sortmode, row_a, row_b );  

    if( ! ret ) ret = compare_col( COL_MARK, SORTMODE_ASCEND, row_a, row_b );
    if( ! ret ) ret = compare_col( COL_ID, SORTMODE_ASCEND, row_a, row_b );

    return ret;
}


//
// コマンド
//
const bool BoardViewBase::set_command( const std::string& command, const std::string& arg1, const std::string& arg2 )
{
    if( command == "update_columns" ) update_columns();

    return true;
}


//
// クロック入力
//
void BoardViewBase::clock_in()
{
    View::clock_in();

    m_treeview.clock_in();
}


//
// ロード停止
//
void BoardViewBase::stop()
{
    DBTREE::board_stop_load( get_url_board() );
}



//
// ビュー表示
//
void BoardViewBase::show_view()
{
#ifdef _DEBUG
    std::cout << "BoardViewBase::show_view " << get_url() << std::endl;
#endif

    // DBに登録されてない
    if( get_url_board().empty() ){
        set_status( "invalid URL" );
        BOARD::get_admin()->set_command( "set_status", get_url(), get_status() );
        return;
    }

    update_boardname();

#if GTKMMVER >= 280
    m_treeview.unset_model();
#endif
    m_liststore->clear();
    m_pre_query = std::string();
    m_last_access_time = DBTREE::board_last_access_time( get_url_board() );
    m_loading = true;
    
    // download 開始
    // 終わったら update_view() が呼ばれる
    DBTREE::board_download_subject( get_url_board(), get_url() );
    set_status( "loading..." );
    BOARD::get_admin()->set_command( "set_status", get_url(), get_status() );

    // タブにアイコンを表示
    BOARD::get_admin()->set_command( "toggle_icon", get_url() );
}


//
// スクロールバー再描画
//
void BoardViewBase::redraw_scrollbar()
{
#ifdef _DEBUG
    std::cout << "BoardViewBase::redraw_scrollbar\n";
#endif

    m_scrwin.queue_draw();
}


//
// 色、フォントの更新
//
void BoardViewBase::relayout()
{
    m_treeview.init_color( COLOR_CHAR_BOARD, COLOR_BACK_BOARD, COLOR_BACK_BOARD_EVEN );
    m_treeview.init_font( CONFIG::get_fontname( FONT_BOARD ) );
}



//
// view更新
//
void BoardViewBase::update_view_impl( std::list< DBTREE::ArticleBase* >& list_subject )
{
    m_updated = false;
    m_loading = false;

#ifdef _DEBUG
    const int code = DBTREE::board_code( get_url_board() );
    std::cout << "BoardViewBase::update_view_impl " << get_url()
              << " code = " << code << std::endl;
#endif    

    // 画面消去
#if GTKMMVER >= 280
    m_treeview.unset_model();
#endif
    m_liststore->clear();

    // 自動ソート抑制
    UNSORTED_COLUMN();

    int id = 1;
    if( list_subject.size() ){

        std::list< DBTREE::ArticleBase* >::iterator it;
        for( it = list_subject.begin(); it != list_subject.end(); ++it, ++id ){

            DBTREE::ArticleBase* art = *( it );
    
            // 行を作って内容をセット
            Gtk::TreeModel::Row row = *( m_liststore->prepend() ); // append より prepend の方が速いらしい

            row[ m_columns.m_col_id ]  = id;
            row[ m_columns.m_col_since ] = art->get_since_date();

            if( art->get_status() & STATUS_NORMAL )
                row[ m_columns.m_col_speed ] = art->get_speed();
        
            row[ m_columns.m_col_since_t ] = art->get_since_time();
            row[ m_columns.m_col_id_dat ] = art->get_id();

            row[ m_columns.m_col_drawbg ] = false;

            update_row_common( art, row );

            // 更新があるかチェック
            if( art->get_number_load() && art->get_number() > art->get_number_load() ) m_updated = true;
        }

#if GTKMMVER >= 280
        m_treeview.set_model( m_liststore );
#endif

        restore_sort();
    }

    // ステータスバー更新
    std::ostringstream ss_tmp;
    ss_tmp << DBTREE::board_str_code( get_url_board() ) << " [ 全 " << ( id -1 ) << " ] ";
    set_status( ss_tmp.str() );
    BOARD::get_admin()->set_command( "set_status", get_url(), get_status() );

    // タブのアイコン状態を更新
    BOARD::get_admin()->set_command( "toggle_icon", get_url() );
}


void BoardViewBase::focus_view()
{
#ifdef _DEBUG
    std::cout << "BoardViewBase::focus_view\n";
#endif

    m_treeview.grab_focus();
}


void BoardViewBase::focus_out()
{
    SKELETON::View::focus_out();

    save_column_width();
    m_treeview.hide_tooltip();
}



//
// 閉じる
//
void BoardViewBase::close_view()
{
    BOARD::get_admin()->set_command( "close_currentview" );
}


//
// 選択した行のログをまとめて削除
//
void BoardViewBase::slot_delete_logs()
{
    std::list< Gtk::TreeModel::iterator > list_it = m_treeview.get_selected_iterators();
    std::list< Gtk::TreeModel::iterator >::iterator it = list_it.begin();
    for( ; it != list_it.end(); ++it ){
        Gtk::TreeModel::Row row = *( *it );
        const std::string url = DBTREE::url_datbase( get_url_board() ) + row[ m_columns.m_col_id_dat ];
#ifdef _DEBUG
        std::cout << url << std::endl;
#endif
        CORE::core_set_command( "delete_article", url );
    }
}




//
// viewの操作
//
const bool BoardViewBase::operate_view( const int control )
{
    bool open_tab = false;

    Gtk::TreePath path = m_treeview.get_current_path();;

    switch( control ){

        case CONTROL::Down:
            row_down();
            break;

        case CONTROL::Up:
            row_up();
            break;

        case CONTROL::PageUp:
            page_up();
            break;

        case CONTROL::PageDown:
            page_down();
            break;

        case CONTROL::Home:
            goto_top();
            break;
            
        case CONTROL::End:
            goto_bottom();
            break;

            // 全て選択
        case CONTROL::SelectAll:
            slot_select_all();
            break;
    
            // スレを開く
        case CONTROL::OpenArticleTab:
            open_tab = true;
        case CONTROL::OpenArticle:
            if( ! path.empty() ) open_row( path, open_tab );
            break;

            // Listに戻る
        case CONTROL::Left:
            CORE::core_set_command( "switch_leftview" );
            break;

            // 現在の記事を表示
        case CONTROL::Right:
            CORE::core_set_command( "switch_rightview" );
            break;

        case CONTROL::ScrollLeftBoard:
            scroll_left();
            break;

        case CONTROL::ScrollRightBoard:
            scroll_right();
            break;

        case CONTROL::ToggleArticle:
            CORE::core_set_command( "toggle_article" );
            break;

        case CONTROL::TabLeft:
            BOARD::get_admin()->set_command( "tab_left" );
            break;

        case CONTROL::TabRight:
            BOARD::get_admin()->set_command( "tab_right" );
            break;

            // 戻る、進む
        case CONTROL::PrevView:
            back_viewhistory( 1 );
            break;

        case CONTROL::NextView:
            forward_viewhistory( 1 );
            break;

        case CONTROL::Quit:
            close_view();
            break;

        case CONTROL::Reload:
            reload();
            break;

        case CONTROL::StopLoading:
            stop();
            break;

        case CONTROL::NewArticle:
            write();
            break;

        case CONTROL::Delete:
        {

            if( CONFIG::get_show_deldiag() ){

                SKELETON::MsgCheckDiag mdiag( NULL,
                                              "選択した行のログを削除しますか？",
                                              "今後表示しない(常に削除)(_D)",
                                              Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO );
                mdiag.set_title( "削除確認" );
                const int ret = mdiag.run();
                if( ret != Gtk::RESPONSE_YES ) return true;
                if( mdiag.get_chkbutton().get_active() ) CONFIG::set_show_deldiag( false );
            }

            slot_delete_logs();
            break;
        }

        // ポップアップメニュー表示
        case CONTROL::ShowPopupMenu:
            show_popupmenu( "", true );
            break;

        // 検索
        case CONTROL::Search:
            m_search_invert = false;
            BOARD::get_admin()->set_command( "focus_toolbar_search" );
            break;

        case CONTROL::SearchInvert:
            m_search_invert = true;
            BOARD::get_admin()->set_command( "focus_toolbar_search" );
            break;

        case CONTROL::SearchNext:
            down_search();
            break;
    
        case CONTROL::SearchPrev:
            up_search();
            break;

            // サイドバー表示/非表示
        case CONTROL::ShowSideBar:
            CORE::core_set_command( "toggle_sidebar" );
            break;

            // メニューバー表示/非表示
        case CONTROL::ShowMenuBar:
            CORE::core_set_command( "toggle_menubar" );
            break;

        default:
            return false;
    }

    return true;
}




//
// 先頭に戻る
//
void BoardViewBase::goto_top()
{
    m_treeview.goto_top();
}


//
// 一番最後へ
//
void BoardViewBase::goto_bottom()
{
    m_treeview.goto_bottom();
}



//
// 指定したIDのスレに移動
//
void BoardViewBase::goto_num( const int num )
{
    if( ! num ) return;

    focus_view();
   
    Gtk::TreeModel::Children child = m_liststore->children();
    Gtk::TreeModel::Children::iterator it = child.begin();
    for( ; it != child.end() ; ++it ){

        Gtk::TreeModel::Row row = *( it );
        
        if( row[ m_columns.m_col_id ] == num ){

            Gtk::TreePath path = GET_PATH( row );
            m_treeview.scroll_to_row( path );
            m_treeview.set_cursor( path );
            return;
        }
    }
}



//
// 左スクロール
//
void BoardViewBase::scroll_left()
{
    Gtk::Adjustment*  hadjust = m_scrwin.get_hadjustment();
    if( !hadjust ) return;
    hadjust->set_value( MAX( 0,  hadjust->get_value() - hadjust->get_step_increment() ) );
}


//
// 右スクロール
//
void BoardViewBase::scroll_right()
{
    Gtk::Adjustment*  hadjust = m_scrwin.get_hadjustment();
    if( !hadjust ) return;
    hadjust->set_value(  MIN( hadjust->get_upper() - hadjust->get_page_size(),
                              hadjust->get_value() + hadjust->get_step_increment() ) );
}


//
// 上へ移動
//
void BoardViewBase::row_up()
{
    m_treeview.row_up();
}    


//
// 下へ移動
//
void BoardViewBase::row_down()
{
    m_treeview.row_down();
} 
   

//
// page up
//
void BoardViewBase::page_up()
{
    m_treeview.page_up();
}    


//
// page down
//
void BoardViewBase::page_down()
{
    m_treeview.page_down();
} 
   

//
// ポップアップメニュー取得
//
// SKELETON::View::show_popupmenu() を参照すること
//
Gtk::Menu* BoardViewBase::get_popupmenu( const std::string& url )
{
    Gtk::Menu* popupmenu = NULL;

    // 削除サブメニュー
    if( url == "popup_menu_delete" ){
        popupmenu = dynamic_cast< Gtk::Menu* >( ui_manager()->get_widget( "/popup_menu_delete" ) );
    }

    // お気に入りサブメニュー
    else if( url == "popup_menu_favorite" ){
        popupmenu = dynamic_cast< Gtk::Menu* >( ui_manager()->get_widget( "/popup_menu_favorite" ) );
    }

    // 通常メニュー
    else if( m_treeview.get_selection()->get_selected_rows().size() == 1 ){
        m_path_selected = * (m_treeview.get_selection()->get_selected_rows().begin() );
        popupmenu = dynamic_cast< Gtk::Menu* >( ui_manager()->get_widget( "/popup_menu" ) );
    }

    // 複数選択メニュー
    else{ 
        m_path_selected = Gtk::TreeModel::Path();
        popupmenu = dynamic_cast< Gtk::Menu* >( ui_manager()->get_widget( "/popup_menu_mul" ) );
    }

    return popupmenu;
}



//
// 特定の行だけの更新
//
void BoardViewBase::update_item( const std::string& url, const std::string& id_dat )
{
    if( get_url().find( url ) != 0 ) return;

#ifdef _DEBUG
    std::cout << "BoardViewBase::update_item " << get_url() << " id = " << id_dat << std::endl;
#endif

    Gtk::TreeModel::Children child = m_liststore->children();
    Gtk::TreeModel::Children::iterator it;

    // 自動ソート抑制
    UNSORTED_COLUMN();
    
    for( it = child.begin() ; it != child.end() ; ++it ){

        Gtk::TreeModel::Row row = *( it );
        
        // 対象の行なら内容更新
        if( row[ m_columns.m_col_id_dat ] == id_dat ){

            const std::string url = DBTREE::url_datbase( get_url_board() ) + row[ m_columns.m_col_id_dat ];
            DBTREE::ArticleBase* art = DBTREE::get_article( url );

            update_row_common( art, row );
        }
    }
}



//
// update_view() と update_item() で共通に更新する列
//
void BoardViewBase::update_row_common( DBTREE::ArticleBase* art, Gtk::TreeModel::Row& row )
{
    if( art->empty() ) return;

    const int load = art->get_number_load();
    const int res = art->get_number();

    // タイトル、レス数、抽出
    row[ m_columns.m_col_subject ] = art->get_subject();
    row[ m_columns.m_col_res ] = res;

    // 読み込み数

    if( load ){
        const int tmpsize = 32;
        char tmp[ tmpsize ];
        snprintf( tmp, tmpsize, "%d", load );
        row[ m_columns.m_col_str_load ] = tmp;
        snprintf( tmp, tmpsize, "%d", res - load );
        row[ m_columns.m_col_str_new ] = tmp;

        row[ m_columns.m_col_load ] = load;
        row[ m_columns.m_col_new ] = res - load;
    }
    else{
        row[ m_columns.m_col_str_load ] = "";
        row[ m_columns.m_col_str_new ] = "";

        row[ m_columns.m_col_load ] = -1;
        row[ m_columns.m_col_new ] = -1;
    }


    //
    // マーク

    int mark_val;

    // ブックマーク & 新着あり
    if( art->is_bookmarked_thread() && art->enable_load() ){
        mark_val = COL_MARKVAL_BKMARKED_UPDATED;
        row[ m_columns.m_col_mark ] = ICON::get_icon( ICON::BKMARK_UPDATE );
        art->show_updateicon( true );
    }

    // ブックマーク
    else if( art->is_bookmarked_thread() ){
        mark_val = COL_MARKVAL_BKMARKED;
        row[ m_columns.m_col_mark ] = ICON::get_icon( ICON::BKMARK );
        art->show_updateicon( false );
    }

    // dat落ち
    else if( art->get_status() & STATUS_OLD ){
        mark_val = COL_MARKVAL_OLD;
        row[ m_columns.m_col_mark ] = ICON::get_icon( ICON::DOWN );
        art->show_updateicon( false );
    }

    // キャッシュはあるが規定のレス数を越えていて全てのレスが既読
    else if( art->is_finished() ){
        mark_val = COL_MARKVAL_FINISHED;
        row[ m_columns.m_col_mark ] = ICON::get_icon( ICON::CHECK );
        art->show_updateicon( false );
    }

    // 新着あり
    else if( art->is_cached() && art->enable_load() ){
        mark_val = COL_MARKVAL_UPDATED;
        row[ m_columns.m_col_mark ] = ICON::get_icon( ICON::UPDATE );
        art->show_updateicon( true );
    }
    // キャッシュあり、新着無し
    else if( art->is_cached() ){
        mark_val = COL_MARKVAL_CACHED;
        row[ m_columns.m_col_mark ] = ICON::get_icon( ICON::CHECK );
        art->show_updateicon( false );
    }
    // キャッシュ無し、新着
    else if( art->get_since_time() > m_last_access_time ){
        mark_val = COL_MARKVAL_NEWTHREAD;
        row[ m_columns.m_col_mark ] = ICON::get_icon( ICON::NEWTHREAD );
    }
    // キャッシュ無し、新着( CONFIG::get_newthread_hour() 時間以内 )
    else if( art->get_hour() < CONFIG::get_newthread_hour() ){
        mark_val = COL_MARKVAL_NEWTHREAD_HOUR;
        row[ m_columns.m_col_mark ] = ICON::get_icon( ICON::NEWTHREAD_HOUR );
    }
    //キャッシュ無し
    else{
        mark_val = COL_MARKVAL_NORMAL;
        row[ m_columns.m_col_mark ] = ICON::get_icon( ICON::TRANSPARENT );
    }
    row[ m_columns.m_col_mark_val ] = mark_val;


    // 書き込み時間
    if( art->get_write_time() ){
        row[ m_columns.m_col_write ] = art->get_write_date();
        row[ m_columns.m_col_write_t ] = art->get_write_time();
    }
    else{
        row[ m_columns.m_col_write ] = std::string();
        if( mark_val < COL_MARKVAL_NORMAL ) row[ m_columns.m_col_write_t ] = 0;
        else row[ m_columns.m_col_write_t ] = -1;
    }
}


//
// マウスボタン押した
//
const bool BoardViewBase::slot_button_press( GdkEventButton* event )
{
    // マウスジェスチャ
    get_control().MG_start( event );

    // ホイールマウスジェスチャ
    get_control().MG_wheel_start( event );

    // ダブルクリック
    // button_release_eventでは event->type に必ず GDK_BUTTON_RELEASE が入る
    m_dblclick = false;
    if( event->type == GDK_2BUTTON_PRESS ) m_dblclick = true; 

    BOARD::get_admin()->set_command( "switch_admin" );

    return true;
}



//
// マウスボタン離した
//
const bool BoardViewBase::slot_button_release( GdkEventButton* event )
{
    /// マウスジェスチャ
    const int mg = get_control().MG_end( event );

    // ホイールマウスジェスチャ
    // 実行された場合は何もしない 
    if( get_control().MG_wheel_end( event ) ) return true;

    if( mg != CONTROL::None && enable_mg() ){
        operate_view( mg );
        return true;
    }

    const int x = (int)event->x;
    const int y = (int)event->y;
    Gtk::TreeModel::Path path;
    Gtk::TreeViewColumn* column;
    int cell_x;
    int cell_y;
   
    // 座標からpath取得
    if( m_treeview.get_path_at_pos( x, y, path, column, cell_x, cell_y ) ){

        m_path_selected = path;

#ifdef _DEBUG        
        std::cout << "BoardViewBase::slot_button_press : path = " << path.to_string()
                  << " x = " << x << " y = " << y
                  << " cellheight = " << m_treeview.get_row_height() 
                  << " cell_x = " << cell_x << " cell_y = " << cell_y << std::endl;
#endif

        // リサイズするときにラベルをクリックすると行を開く問題の対処
        // かなりその場しのぎな方法なのでGTKのバージョンが上がったら誤動作するかも
        if( x == cell_x && y < m_treeview.get_row_height() ) return true;

        // ダブルクリックの処理のため一時的にtypeを切替える
        GdkEventType type_copy = event->type;
        if( m_dblclick ) event->type = GDK_2BUTTON_PRESS;

        // スレを開く
        bool openarticle = get_control().button_alloted( event, CONTROL::OpenArticleButton );
        bool openarticletab = get_control().button_alloted( event, CONTROL::OpenArticleTabButton );

        if( openarticle || openarticletab ){

            // 複数行選択中
            if( m_treeview.get_selected_iterators().size() >= 2 ) open_selected_rows();

            else open_row( path, openarticletab );
        }

        // ポップアップメニューボタン
        else if( get_control().button_alloted( event, CONTROL::PopupmenuButton ) ){

            show_popupmenu( "", false );
        }

        else operate_view( get_control().button_press( event ) );

        event->type = type_copy;
    }

    return true;
}



//
// マウス動かした
//
const bool BoardViewBase::slot_motion_notify( GdkEventMotion* event )
{
    /// マウスジェスチャ
    get_control().MG_motion( event );

    const int x = (int)event->x;
    const int y = (int)event->y;
    Gtk::TreeModel::Path path;
    Gtk::TreeView::Column* column;
    int cell_x;
    int cell_y;

    // ツールチップに文字列をセットする
    if( m_treeview.get_path_at_pos( x, y, path, column, cell_x, cell_y ) ){

        m_treeview.set_tooltip_min_width( column->get_width() );
        if( column->get_title() == ITEM_NAME_NAME ) m_treeview.set_str_tooltip( get_name_of_cell( path, m_columns.m_col_subject ) );
        else if( column->get_title() == ITEM_NAME_SINCE ) m_treeview.set_str_tooltip( get_name_of_cell( path, m_columns.m_col_since ) );
        else if( column->get_title() == ITEM_NAME_LASTWRITE ) m_treeview.set_str_tooltip( get_name_of_cell( path, m_columns.m_col_write ) );
        else m_treeview.set_str_tooltip( std::string() );
    }

    return true;
}




//
// キー入力
//
const bool BoardViewBase::slot_key_press( GdkEventKey* event )
{
    m_pressed_key = get_control().key_press( event );

    if( m_pressed_key != CONTROL::None ){

        // キー入力でスレを開くとkey_releaseイベントがboadviewが画面から
        // 消えてから送られてWIDGET_REALIZED_FOR_EVENT assertionが出るので
        // OpenArticle(Tab)は slot_key_release() で処理する
        if( m_pressed_key == CONTROL::OpenArticle ) return true;
        if( m_pressed_key == CONTROL::OpenArticleTab ) return true;

        if( operate_view( m_pressed_key ) ) return true;
    }
    else if( release_keyjump_key( event->keyval ) ) return true;

    return false;
}


//
// キーリリースイベント
//
const bool BoardViewBase::slot_key_release( GdkEventKey* event )
{
    const int key = get_control().key_press( event );

    // 押したキーと違うときは処理しない
    if( key == m_pressed_key ){

        // キー入力でスレを開くとkey_releaseイベントがboadviewが画面から
        // 消えてから送られてWIDGET_REALIZED_FOR_EVENT assertionが出るので
        // OpenArticle(Tab)はここで処理する
        if( key == CONTROL::OpenArticle ) operate_view( key );
        if( key == CONTROL::OpenArticleTab ) operate_view( key );
    }
   
    return true;
}



//
// マウスホイールイベント
//
const bool BoardViewBase::slot_scroll_event( GdkEventScroll* event )
{
    // ホイールマウスジェスチャ
    const int control = get_control().MG_wheel_scroll( event );
    if( enable_mg() && control != CONTROL::None ){
        operate_view( control );
        return true;
    }

    m_treeview.wheelscroll( event );
    return true;
}



//
// このビューからD&Dを開始したときにtreeviewから呼ばれる
//
void BoardViewBase::slot_drag_begin()
{
#ifdef _DEBUG    
    std::cout << "BoardViewBase::slot_drag_begin\n";
#endif

    CORE::DND_Begin( get_url() );
    set_article_to_buffer();
}



//
// このビューからD&Dを開始した後にD&Dを終了するとtreeviewから呼ばれる
//
void BoardViewBase::slot_drag_end()
{
#ifdef _DEBUG    
    std::cout << "BoardViewBase::slot_drag_end\n";
#endif

    CORE::DND_End();
}



//
// ブックマーク設定、解除
//
void BoardViewBase::slot_bookmark( const int bookmark )
{
    const std::string datbase = DBTREE::url_datbase( get_url_board() );
    std::list< Gtk::TreeModel::iterator > list_it = m_treeview.get_selected_iterators();
    std::list< Gtk::TreeModel::iterator >::iterator it = list_it.begin();
    for( ; it != list_it.end(); ++it ){
        Gtk::TreeModel::Row row = *( *it );
        const std::string url = datbase + row[ m_columns.m_col_id_dat ];
        DBTREE::ArticleBase* art = DBTREE::get_article( url );
        if( art ){
            bool set = bookmark;
            if( bookmark == BOOKMARK_AUTO ) set = ! art->is_bookmarked_thread();
#ifdef _DEBUG
            std::cout << "BoardViewBase::slot_bookmark url = " << url << " set = " << set << std::endl;
#endif
            art->set_bookmarked_thread( set );
        }
    }
}


//
// popupmenu でタブで開くを選択
//
void BoardViewBase::slot_open_tab()
{
    if( ! m_path_selected.empty() ) open_row( m_path_selected, true );
}


//
// スレをお気に入りに登録
//
// ポップアップメニューのslot
//
void BoardViewBase::slot_favorite_thread()
{
    // 共有バッファにデータをセットしてから append_favorite コマンド実行
    set_article_to_buffer();
    CORE::core_set_command( "append_favorite", URL_FAVORITEVIEW );
}




//
// 板をお気に入りに追加
//
void BoardViewBase::slot_favorite_board()
{
    // 共有バッファにデータをセットしてから append_favorite コマンド実行
    set_board_to_buffer();
    CORE::core_set_command( "append_favorite", URL_FAVORITEVIEW );
}


//
// 新スレをたてる
//
void BoardViewBase::write()
{
    CORE::core_set_command( "create_new_thread", get_url_board() );
}


//
// ツールバーの削除ボタン
//
void BoardViewBase::delete_view()
{
    show_popupmenu( "popup_menu_delete", false );
}


//
// ツールバーのお気に入りボタン
//
void BoardViewBase::set_favorite()
{
    show_popupmenu( "popup_menu_favorite", false );
}


//
// スレのURLをコピー
//
void BoardViewBase::slot_copy_url()
{
    if( m_path_selected.empty() ) return;

    const std::string url = DBTREE::url_readcgi( path2daturl( m_path_selected ), 0, 0 );
    MISC::CopyClipboard( url );
}


// スレの名前とURLをコピー
//
void BoardViewBase::slot_copy_title_url()
{
    if( m_path_selected.empty() ) return;

    const std::string url = DBTREE::url_readcgi( path2daturl( m_path_selected ), 0, 0 );
    const std::string name = DBTREE::article_subject( url );
    std::stringstream ss;
    ss << name << std::endl
       << url << std::endl;

    MISC::CopyClipboard( ss.str() );
}


//
// 全選択
//
void BoardViewBase::slot_select_all()
{
    SKELETON::MsgDiag mdiag( NULL, "全ての行を選択しますか？", false, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_YES_NO );
    mdiag.set_default_response( Gtk::RESPONSE_NO );
    if( mdiag.run() != Gtk::RESPONSE_YES ) return;

    Gtk::TreeModel::Children child = m_liststore->children();
    Gtk::TreeModel::Children::iterator it = child.begin();
    for( ; it != child.end() ; ++it ) m_treeview.get_selection()->select( *it );
}


//
// ポップアップメニューでブラウザで開くを選択
//
void BoardViewBase::slot_open_browser()
{
    const std::string url = DBTREE::url_readcgi( path2daturl( m_path_selected ), 0, 0 );
    CORE::core_set_command( "open_url_browser", url );
}



//
// 記事を開く 
//
const bool BoardViewBase::open_row( Gtk::TreePath& path, const bool tab )
{
    std::string str_tab = "false";
    if( tab ) str_tab = "true";

    const std::string url_target = path2daturl( path );

#ifdef _DEBUG
    std::cout << "BoardViewBase::open_row " << url_target << std::endl;
#endif

    if( url_target.empty() ) return false;

    // datロード終了時に次スレ移行チェックを行う
    DBTREE::article_set_url_pre_article( url_target, get_url_pre_article() );

    const std::string mode = std::string();
    CORE::core_set_command( "open_article", url_target, str_tab, mode );
    return true;
}



//
// 選択した行をまとめて開く
//
void BoardViewBase::open_selected_rows()
{
    std::string list_url;
    std::list< Gtk::TreeModel::iterator > list_it = m_treeview.get_selected_iterators();
    std::list< Gtk::TreeModel::iterator >::iterator it = list_it.begin();
    for( ; it != list_it.end(); ++it ){
        Gtk::TreeModel::Row row = *( *it );
        const std::string url = DBTREE::url_datbase( get_url_board() ) + row[ m_columns.m_col_id_dat ];

        if( !list_url.empty() ) list_url += " ";
        list_url += url;

        // datロード終了時に次スレ移行チェックを行う
        DBTREE::article_set_url_pre_article( url, get_url_pre_article() );
    }

    CORE::core_set_command( "open_article_list", std::string(), list_url );
}


//
// path -> スレッドの(dat型)URL変換
// 
const std::string BoardViewBase::path2daturl( const Gtk::TreePath& path )
{
    Gtk::TreeModel::Row row = m_treeview.get_row( path );
    if( !row ) return std::string();

    const std::string url = DBTREE::url_datbase( get_url_board() ) + row[ m_columns.m_col_id_dat ];
    return url;
}




//
// 抽出
//
const bool BoardViewBase::drawout()
{
    int hit = 0;
    bool reset = false;

    const std::string query = get_search_query();

    // 空の時はリセット
    if( query.empty() ) reset = true;

#ifdef _DEBUG
    std::cout << "BoardViewBase::drawout query = " <<  query << std::endl;
#endif

    // 自動ソート抑制
    UNSORTED_COLUMN();

    JDLIB::Regex regex;
    Gtk::TreeModel::Children child = m_liststore->children();
    Gtk::TreeModel::Children::iterator it = child.begin();

    if ( ! reset ) regex.compile( query, true, true, true );

    for( ; it != child.end() ; ++it ){

        Gtk::TreeModel::Row row = *( it );
        const Glib::ustring subject = row[ m_columns.m_col_subject ];

        if( reset ) row[ m_columns.m_col_drawbg ] = false;
        else if( regex.exec( subject, 0 ) ){
            row[ m_columns.m_col_drawbg ] = true;
            ++hit;

#ifdef _DEBUG
            std::cout << subject << " " << row[ m_columns.m_col_mark_val ] << std::endl;
#endif

        }
        else row[ m_columns.m_col_drawbg ] = false;
    }

    restore_sort();

    if( reset ) CORE::core_set_command( "set_info", "", "" );
    else if( ! hit ) CORE::core_set_command( "set_info", "", "検索結果： ヒット無し" );
    else CORE::core_set_command( "set_info", "", "検索結果： " + MISC::itostr( hit ) + "件" );

    return true;
}



//
// 検索ボックスの文字列が変わった
//
void BoardViewBase::set_search_query( const std::string& query )
{
    SKELETON::View::set_search_query( query );

#ifdef _DEBUG
    std::cout << "BoardViewBase::set_search_query query = " << get_search_query() << std::endl;
#endif

    if( CONFIG::get_inc_search_board() ){
        drawout();
        m_pre_query = std::string();
    }
}


//
// 検索実行
//
void BoardViewBase::exec_search()
{
    const std::string query = get_search_query();
    if( m_pre_query != query ){
        drawout();
        focus_view();
        m_pre_query = query;
        CORE::get_completion_manager()->set_query( CORE::COMP_SEARCH_BOARD, query );
        return;
    }

    focus_view();
    if( query.empty() ) return;
   
    Gtk::TreePath path = m_treeview.get_current_path();;
    if( path.empty() ){
        if( m_search_invert ) path = GET_PATH( *( m_liststore->children().begin() ) );
        else GET_PATH( *( m_liststore->children().rbegin() ) );
    }

    Gtk::TreePath path_start = path;
    JDLIB::Regex regex; 

#ifdef _DEBUG
    std::cout << "BoardViewBase::search start = " << path_start.to_string() << " query = " <<  query << std::endl;
#endif
    
    for(;;){

        if( !m_search_invert ){
            // 次へ
            path.next();
            // 先頭に戻る
            if( ! m_treeview.get_row( path ) ) path =  GET_PATH( *( m_liststore->children().begin() ) );
        }
        else{
            // 前へ
            if( ! path.prev() ){
                // 一番後へ
                path =  GET_PATH( *( m_liststore->children().rbegin() ) );
            }
        }

        
        if( path == path_start ) break;
        
        Glib::ustring subject = get_name_of_cell( path, m_columns.m_col_subject );
        if( regex.exec( query, subject, 0, true, true, true ) ){
            m_treeview.scroll_to_row( path, 0 );
            m_treeview.set_cursor( path );
            return;
        }
    }
}




// 前検索
void BoardViewBase::up_search()
{
    m_search_invert = true;
    exec_search();
}



// 後検索
void BoardViewBase::down_search()
{
    m_search_invert = false;
    exec_search();
}


//
// 検索entryの操作
//
void BoardViewBase::operate_search( const std::string& controlid )
{
    const int id = atoi( controlid.c_str() );

    if( id == CONTROL::Cancel ) focus_view();
    else if( id == CONTROL::SearchCache ) CORE::core_set_command( "open_article_searchlog", get_url_board() , get_search_query() );
}


//
// 板プロパティ表示
//
void BoardViewBase::show_preference()
{
    SKELETON::PrefDiag* pref =  CORE::PrefDiagFactory( NULL, CORE::PREFDIAG_BOARD, get_url_board() );
    pref->run();
    delete pref;
}


//
// スレプロパティ表示
//
void BoardViewBase::slot_preferences_article()
{
    if( m_path_selected.empty() ) return;
    const std::string url = path2daturl( m_path_selected );

    SKELETON::PrefDiag* pref= CORE::PrefDiagFactory( NULL, CORE::PREFDIAG_ARTICLE, url );
    pref->run();
    delete pref;
}


//
// 戻る
//
void BoardViewBase::back_viewhistory( const int count )
{
    BOARD::get_admin()->set_command( "back_viewhistory", get_url(), MISC::itostr( count ) );
}


//
// 進む
//
void BoardViewBase::forward_viewhistory( const int count )
{
    BOARD::get_admin()->set_command( "forward_viewhistory", get_url(), MISC::itostr( count ) );
}



//
// datを保存
//
void BoardViewBase::slot_save_dat()
{
    if( m_path_selected.empty() ) return;
    const std::string url = path2daturl( m_path_selected );

    DBTREE::article_save_dat( url, std::string() );
}


//
// 選択したスレをあぼーん
//
void BoardViewBase::slot_abone_thread()
{
    std::list< Gtk::TreeModel::iterator > list_it = m_treeview.get_selected_iterators();
    std::list< Gtk::TreeModel::iterator >::iterator it = list_it.begin();
    if( ! list_it.size() ) return;

    std::list< std::string > threads = DBTREE::get_abone_list_thread( get_url_board() );

    for( ; it != list_it.end(); ++it ){
        Gtk::TreeModel::Row row = *( *it );
        Glib::ustring subject = row[ m_columns.m_col_subject ];
        threads.push_back( subject );
    }

    // あぼーん情報更新
    // 板の再描画も行われる
    std::list< std::string > words = DBTREE::get_abone_list_word_thread( get_url_board() );
    std::list< std::string > regexs = DBTREE::get_abone_list_regex_thread( get_url_board() );
    const int number = DBTREE::get_abone_number_thread( get_url_board() );
    const int hour = DBTREE::get_abone_hour_thread( get_url_board() );
    DBTREE::reset_abone_thread( get_url_board(), threads, words, regexs, number, hour );
}



//
// path と column からそのセルの内容を取得
//
template < typename ColumnType >
const std::string BoardViewBase::get_name_of_cell( Gtk::TreePath& path, const Gtk::TreeModelColumn< ColumnType >& column )
{
    Gtk::TreeModel::Row row = m_treeview.get_row( path );
    if( !row ) return std::string();

    const Glib::ustring name = row[ column ];
    return name;
}



//
// 共有バッファに選択中の行を登録する
//
void BoardViewBase::set_article_to_buffer()
{
    CORE::SBUF_clear_info();

    std::list< Gtk::TreeModel::iterator > list_it = m_treeview.get_selected_iterators();
    if( list_it.size() ){
        std::list< Gtk::TreeModel::iterator >::iterator it = list_it.begin();
        for( ; it != list_it.end(); ++it ){

            Gtk::TreeModel::Row row = *( *it );
            const Glib::ustring name = row[ m_columns.m_col_subject ];

            CORE::DATA_INFO info;
            info.type = TYPE_THREAD;
            info.url = DBTREE::url_datbase( get_url_board() ) + row[ m_columns.m_col_id_dat ];
            info.name = name.raw();

            CORE::SBUF_append( info );
#ifdef _DEBUG    
            std::cout << "append " << info.name << std::endl;
#endif
        }
    }
}



//
// 共有バッファに板を登録する
//
void BoardViewBase::set_board_to_buffer()
{
    CORE::DATA_INFO info;
    info.type = TYPE_BOARD;
    info.url = get_url();
    info.name = DBTREE::board_name( get_url_board() );

    CORE::SBUF_clear_info();
    CORE::SBUF_append( info );
}