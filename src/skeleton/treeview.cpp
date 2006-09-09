// ライセンス: 最新のGPL

//#define _DEBUG
#include "jddebug.h"

#include "treeview.h"
#include "view.h"
#include "popupwin.h"

#include "config/globalconf.h"

#include "controlid.h"

#ifndef MAX
#define MAX( a, b ) ( a > b ? a : b )
#endif


#ifndef MIN
#define MIN( a, b ) ( a < b ? a : b )
#endif


// row -> tree 変換
#define GET_PATH( row ) get_model()->get_path( row )

using namespace SKELETON;


JDTreeView::JDTreeView( const std::string& fontname, const int *rgb )
    : m_reorderable( 0 ),
      m_drag( 0 ),
      m_popup_win( 0 ),
      m_column_for_height( 0 )
{
#ifdef _DEBUG
    std::cout << "JDTreeView::JDTreeView\n";
#endif

    set_enable_search( false );
    set_rules_hint( false );
    add_events( Gdk::BUTTON_PRESS_MASK );
    add_events( Gdk::KEY_PRESS_MASK );
    add_events( Gdk::KEY_RELEASE_MASK );
    add_events( Gdk::POINTER_MOTION_MASK );
    add_events( Gdk::LEAVE_NOTIFY_MASK );
    add_events( Gdk::SCROLL_MASK );

    init_color( rgb );
    init_font( fontname );

    get_selection()->set_mode( Gtk::SELECTION_MULTIPLE );
}


JDTreeView::~JDTreeView()
{
    delete_popup();
}



//
// 色初期化
//
void JDTreeView::init_color( const int *rgb )
{
    // 背景色
    Gdk::Color color;
    color.set_rgb( rgb[ 0 ], rgb[ 1 ], rgb[ 2 ] );
    modify_base( get_state(), color );
}


//
// フォント初期化
//
void JDTreeView::init_font( const std::string& fontname )
{
    Pango::FontDescription pfd( fontname );
    pfd.set_weight( Pango::WEIGHT_NORMAL );
    modify_font( pfd );
}


//
// D&D可にする
//
void JDTreeView::set_reorderable_view( bool reorderable )
{
    if( reorderable ){

        m_reorderable = true;
        std::list< Gtk::TargetEntry > targets;
        targets.push_back( Gtk::TargetEntry( "text/plain", Gtk::TARGET_SAME_APP, 0 ) );

        // ドラッグ開始ボタン設定
        // タブで開くボタンを左クリックに割り当てていたらドラッグ開始ボタンを中ボタンにする
        Gdk::ModifierType type = Gdk::BUTTON1_MASK;
        GdkEventButton event;
        m_control.get_eventbutton( CONTROL::DragStartButton, event );
        if( event.button == 2 ) type = Gdk::BUTTON2_MASK;

        drag_source_set( targets, type );
        drag_dest_set( targets );
    }
    else{
        m_reorderable = false;
        drag_source_unset();
        drag_dest_unset();
    }
}



//
// クロック入力
//
void JDTreeView::clock_in()
{
    m_tooltip.clock_in();
}



//
// ツールチップに文字をセット
//
void JDTreeView::set_str_tooltip( const std::string& text )
{
    m_tooltip.set_text( text );
}


//
// ツールチップ最小幅設定
//
void JDTreeView::set_tooltip_min_width( const int& min_width )
{
    m_tooltip.set_min_width( min_width);
}


//
// ツールチップを表示
//
void JDTreeView::show_tooltip()
{
    m_tooltip.show_tooltip();
}


//
// ツールチップ隠す
//
void JDTreeView::hide_tooltip()
{
    m_tooltip.hide_tooltip();
}


//
// ポップアップウィンドウ表示
//
void JDTreeView::show_popup( const std::string& url, View* view )
{
    const int mrg = 10;

    if( m_popup_win == NULL || m_popup_win->view() != view ){
        delete_popup();
        m_popup_win = new PopupWin( this, view, mrg );
        m_pre_popup_url = url;
    }
}


//
// ポップアップウィンドウ削除
//
void JDTreeView::delete_popup()
{
    if( m_popup_win ){
        delete m_popup_win;
        m_popup_win = NULL;
        m_pre_popup_url = std::string();
    }
}




//
// 先頭へ
//
void JDTreeView::goto_top()
{
    Gtk::TreePath path = GET_PATH( *( get_model()->children().begin() ) );
    scroll_to_row( path, 0 );
    set_cursor( path );
}


//
// 一番最後へ
//
void JDTreeView::goto_bottom()
{
    Gtk::TreePath path = GET_PATH( *( get_model()->children().rbegin() ) );
    scroll_to_row( path, 0 );
    set_cursor( path );
}



//
// 選択行を上へ移動
//
void JDTreeView::row_up()
{
    Gtk::TreePath path = get_current_path();
    if( !get_row( path ) ) return;

    path = prev_path( path );
    set_cursor( path );
}    



//
// 選択行を下へ移動
//
void JDTreeView::row_down()
{
    Gtk::TreePath path = get_current_path();
    if( !get_row( path ) ) return;

    path = next_path( path );
    if( path.get_depth() && get_row( path ) ) set_cursor( path );
} 
   


//
// page up
//
void JDTreeView::page_up()
{
    bool set_top = false;

    // スクロール
    Gtk::Adjustment *adj = get_vadjustment();
    double val = adj->get_value();
    if( val > adj->get_page_size()/2 ) set_top = true;
    val = MAX( 0, val - adj->get_page_size() );
    adj->set_value( val );

    // 選択行移動
    Gtk::TreePath path;
    if( set_top ) path = get_path_under_xy( 0, (int)adj->get_page_size() - 4 );
    else path = get_path_under_xy( 0, 0 );
    if( path.get_depth() && get_row( path ) )set_cursor( path );
}


//
// page down
//
void JDTreeView::page_down()
{
    bool set_bottom = false;

    // スクロール
    Gtk::Adjustment *adj = get_vadjustment();
    double val = adj->get_value();
    if( val < adj->get_upper() - adj->get_page_size() - adj->get_page_size()/2 ) set_bottom = true;
    val = MIN( adj->get_upper() - adj->get_page_size(), val + adj->get_page_size() );
    adj->set_value( val );

    // 選択行移動
    Gtk::TreePath path;
    if( set_bottom ) path = get_path_under_xy( 0, 0 );
    else path = get_path_under_xy( 0, (int)adj->get_page_size() - 4 );
    if( path.get_depth() && get_row( path ) ) set_cursor( path );
}



//
// path の前の path を取得
//
// check_expand = true なら行が開いてるかチェックして開いて無い時はdown()しない
//
Gtk::TreePath JDTreeView::prev_path( const Gtk::TreePath& path, bool check_expand )
{
    Gtk::TreePath path_out( path );

    // 前に移動
    if( path_out.prev() && ( row_expanded( path_out ) || ! check_expand ) ){

        Gtk::TreePath path_tmp = path_out;
        while( get_row( path_out ) && ( path_out = next_path( path_out, check_expand ) ) != path ) path_tmp = path_out;
        if( get_row( path_tmp ) ) return path_tmp;
    }

    // 一番上まで到達したらup
    path_out = path;
    if( ! path_out.prev() && path_out.get_depth() >= 2 ) path_out.up();

    return path_out;;
}



//
// path の次の path を取得
//
// check_expand = true なら行が開いてるかチェックして開いて無い時はdown()しない
//
Gtk::TreePath JDTreeView::next_path( const Gtk::TreePath& path, bool check_expand )
{
    if( !get_row( path ) ) return path;
    Gtk::TreePath path_out( path );

    if( row_expanded( path_out ) || ! check_expand ){
        path_out.down();
        if( get_row( path_out ) ) return path_out;
    }

    // next()してレベルの一番下まで到達したら上のレベルに移動
    path_out = path;
    while( path_out.next(), ( ! get_row( path_out ) && path_out.get_depth() >=2 ) ) path_out.up();

    return path_out;
}




//
// path->row 変換
//
Gtk::TreeModel::Row JDTreeView::get_row( const Gtk::TreePath& path )
{
    if( path.empty() || ! get_model() ) return Gtk::TreeModel::Row();

    Gtk::TreeModel::Row row = *( get_model()->get_iter( path ) );
    if( !row ) return Gtk::TreeModel::Row();
    if( path != get_model()->get_path( row ) ) return Gtk::TreeModel::Row();

    return row;
}



//
// pathの親を再起的に開く
//
void JDTreeView::expand_parents( const Gtk::TreePath& path )
{
    for( int level = 1; level < path.get_depth(); ++level ){
                    
        Gtk::TreeModel::Row row_tmp = get_row( path );
        for( int i = 0; i < path.get_depth() - level; ++i ){
            if( row_tmp.parent() ) row_tmp = *( row_tmp.parent() );
        }
        Gtk::TreePath path_tmp  = GET_PATH( row_tmp );
        expand_row( path_tmp, false );
    }
}




//
// マウスボタンを押した
//
bool JDTreeView::on_button_press_event( GdkEventButton* event )
{
    Gtk::TreeModel::Path path = get_path_under_xy( (int)event->x, (int)event->y );
    m_drag = false;
    m_sig_button_press.emit( event );

    // ドラッグして範囲選択
    // m_path_dragstart が empty でない時に範囲選択を行う
    // on_motion_notify_event()も参照せよ
    if( m_control.button_alloted( event, CONTROL::TreeRowSelectionButton ) )
        m_path_dragstart = m_path_dragpre = path;
    else m_path_dragstart = m_path_dragpre = Gtk::TreeModel::Path();

    // 複数行選択時の動作
    if( get_selection()->get_selected_rows().size() >= 2 ){

        // ポップアップメニューボタンを押しても選択解除しない
        if( m_control.button_alloted( event, CONTROL::PopupmenuButton ) ) return true;

        // D&Dのため、ctrlやshiftなしで普通にクリックしたときも選択解除しない
        if( !( event->state & GDK_CONTROL_MASK )
            && !( event->state & GDK_SHIFT_MASK ) ){

            // ただし範囲選択開始ボタンを押したときは選択解除
            if( !m_path_dragstart.empty() ){
                get_selection()->unselect_all();
                set_cursor( path );
            }

            return true;
        }
    }

    return Gtk::TreeView::on_button_press_event( event );
}



//
// マウスボタンを離した
//
bool JDTreeView::on_button_release_event( GdkEventButton* event )
{
    Gtk::TreeModel::Path path = get_path_under_xy( (int)event->x, (int)event->y );

    if( !( event->state & GDK_CONTROL_MASK )
        && !( event->state & GDK_SHIFT_MASK ) // ctrl/shift + クリックで複数選択してない場合

        && !( !m_path_dragstart.empty() && path != m_path_dragstart ) // ドラッグして範囲選択していない場合
        ){ 

        // 何もないところをクリックしたら選択解除
        if( !get_row( path ) ) get_selection()->unselect_all();

        // ドラッグ中で無ければ選択
        if( get_row( path ) && !m_drag ){

            // クリックした部分が範囲選択部分でないときは選択解除
            bool included = false;
            std::list< Gtk::TreeModel::iterator > list_it = get_selected_iterators();
            std::list< Gtk::TreeModel::iterator >::iterator it = list_it.begin();
            for( ; it != list_it.end(); ++it ){

                Gtk::TreeModel::Row row_tmp = *( *it );
                Gtk::TreePath path_tmp = GET_PATH( row_tmp );

                if( path == path_tmp ){
                    included = true;
                    break;
                }
            }

            if( ! included ) set_cursor( path );
        }


        if( !m_drag ) m_sig_button_release.emit( event );
    }

    m_path_dragstart = m_path_dragpre = Gtk::TreeModel::Path();

    return Gtk::TreeView::on_button_release_event( event );
}



//
// D&D開始
//
// このtreeがソースで無い時は呼ばれないのに注意
//
void JDTreeView::on_drag_begin( const Glib::RefPtr< Gdk::DragContext >& context )
{
    Gtk::TreeModel::Path path = get_path_under_mouse();

#ifdef _DEBUG
    std::cout << "JDTreeView::on_drag_begin path = " << path.to_string() << std::endl;
#endif

    m_drag = true;
    m_sig_drag_begin.emit();

    return Gtk::TreeView::on_drag_begin( context );
}


//
// D&D中にマウスを動かした
//
// 他のwidgetがソースの時も呼ばれるのに注意
// またドラッグ中は on_motion_notify_event() は呼ばれない
//
bool JDTreeView::on_drag_motion( const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time )
{

    Gtk::TreeModel::Path path = get_path_under_mouse();
#ifdef _DEBUG
    std::cout << "JDTreeView::on_drag_motion x = " << x << " y = " << y << " path = " << path.to_string() << std::endl;
#endif

    m_sig_drag_motion.emit( path );

    return Gtk::TreeView::on_drag_motion( context, x, y, time );
}


//
// D&Dでドロップされた
//
// 他のwidgetがソースの時も呼ばれるのに注意
//
bool JDTreeView::on_drag_drop( const Glib::RefPtr<Gdk::DragContext>& context, int x, int y, guint time )
{
#ifdef _DEBUG
    std::cout << "JDTreeView::on_drag_drop\n";
#endif

    Gtk::TreeModel::Path path = get_path_under_mouse();
#ifdef _DEBUG
    std::cout << "dest x = " << x << " y = " << y << " path = " << path.to_string() << std::endl;
#endif    
    m_sig_drag_drop.emit( path );

    return Gtk::TreeView::on_drag_drop( context, x, y, time );
}


//
// D&D 終了
//
// このtreeがソースで無い時は呼ばれないのに注意
//
void JDTreeView::on_drag_end( const Glib::RefPtr< Gdk::DragContext >& context )
{
#ifdef _DEBUG
    std::cout << "JDTreeView::on_drag_end\n";
#endif

    m_drag = false;
    m_sig_drag_end.emit();

    return Gtk::TreeView::on_drag_end( context );
}



bool JDTreeView::on_key_press_event( GdkEventKey* event )
{
    m_sig_key_press.emit( event );

    return true;
}


bool JDTreeView::on_key_release_event( GdkEventKey* event )
{
    m_sig_key_release.emit( event );

    return true;
}


//
// マウスを動かした
//
bool JDTreeView::on_motion_notify_event( GdkEventMotion* event )
{
#ifdef _DEBUG
//    std::cout << "JDTreeView::on_motion_notify_event x = " << event->x << " y = " << event->y << std::endl;
#endif

    // drag_source_set() でセットしたボタン以外でドラッグして範囲選択
    // m_path_dragstart が empty で無いときに実行
    // JDTreeView::on_button_press_event() も参照せよ
    Gtk::TreeModel::Path path = get_path_under_xy( (int)event->x, (int)event->y );
    if( ! m_path_dragstart.empty() && !path.empty() && path != m_path_dragpre ){
        get_selection()->unselect_all();
        get_selection()->select( path, m_path_dragstart );
        m_path_dragpre = path;
    }

    m_sig_motion.emit( event );

    return Gtk::TreeView::on_motion_notify_event( event );
}


// マウスのwheelを回した
bool JDTreeView::on_scroll_event( GdkEventScroll* event )
{
    Gtk::Adjustment *adj = get_vadjustment();
    double val = adj->get_value();

    int scr_inc = get_row_height() * CONFIG::get_tree_scroll_size();

    if( event->direction == GDK_SCROLL_UP ) val = MAX( 0, val - scr_inc );
    else if( event->direction == GDK_SCROLL_DOWN ) val = MIN( adj->get_upper() - adj->get_page_size(), val + scr_inc );
    adj->set_value( val );

#ifdef _DEBUG
    std::cout << "JDTreeView::on_scroll_event\n";

    std::cout << "scr_inc = " << scr_inc << std::endl;
    std::cout << "lower = " << adj->get_lower() << std::endl;
    std::cout << "upper = " << adj->get_upper() << std::endl;
    std::cout << "value = " << val << std::endl;
    std::cout << "step = " << adj->get_step_increment() << std::endl;
    std::cout << "page = " << adj->get_page_increment() << std::endl;
    std::cout << "page_size = " << adj->get_page_size() << std::endl;
#endif

    return true;
}



bool JDTreeView::on_leave_notify_event( GdkEventCrossing* event )
{
    m_tooltip.hide_tooltip();
    delete_popup();
    return Gtk::TreeView::on_leave_notify_event( event );
}




//
// 現在フォーカスしてる行の最初のパスを取得
//
Gtk::TreeModel::Path JDTreeView::get_current_path()
{
    Gtk::TreeModel::Path path;
    
    std::list< Gtk::TreeModel::Path > paths = get_selection()->get_selected_rows();
    if( paths.size() ){
    
        std::list< Gtk::TreeModel::Path >::iterator it = paths.begin();
        path = ( *it );
    }

    return path;
}



//
// x, y 座標の下のパスを取得
//
Gtk::TreeModel::Path JDTreeView::get_path_under_xy( int x, int y )
{
    Gtk::TreeModel::Path path;
    Gtk::TreeViewColumn* column;
    int cell_x, cell_y;
    if( !get_path_at_pos( x, y, path, column, cell_x, cell_y ) ) return Gtk::TreeModel::Path();

    return path;
}



//
// 現在のマウスポインタの下のパスを取得
//
Gtk::TreeModel::Path JDTreeView::get_path_under_mouse()
{
    int x, y;
    get_pointer( x, y );
    return get_path_under_xy( x, y );
}


//
// 現在のマウスポインタの下のセルの幅高さとセル内での座標を取得
//
void JDTreeView::get_cell_xy_wh( int& cell_x, int& cell_y, int& cell_w, int& cell_h )
{
    cell_x = cell_y = cell_w = cell_h = -1;

    Gtk::TreeModel::Path path;
    Gtk::TreeViewColumn* column;
    int x, y, o_x, o_y;
    Gdk::Rectangle rect;

    get_pointer( x, y );
    get_path_at_pos( x, y, path, column, cell_x, cell_y );
    if( column ) column->cell_get_size( rect, o_x, o_y, cell_w, cell_h );
}


//
// 行のセルの高さ
//
int JDTreeView::get_row_height()
{
    Gtk::TreeViewColumn* column = get_column( m_column_for_height );
    if( !column ) return 0;

    int x,y,w,h;
    Gdk::Rectangle rect;
    column->cell_get_size( rect, x, y, w, h );

    return h;
}



//
// 選択中の Gtk::TreeModel::iterator のリストを取得
//
// 削除などを実行してから get_model()->get_iter() するとパスが変わってエラーが出るので
// 先に iterator だけ取得しておく
//
std::list< Gtk::TreeModel::iterator > JDTreeView::get_selected_iterators()
{
    std::list< Gtk::TreeModel::iterator > list_it;
    
    if( get_model() ){

        std::list< Gtk::TreeModel::Path > paths = get_selection()->get_selected_rows();
        std::list< Gtk::TreeModel::Path >::iterator it = paths.begin();
        for( ; it != paths.end(); ++it ) list_it.push_back( get_model()->get_iter( ( *it ) ) );
    }

    return list_it;
}


