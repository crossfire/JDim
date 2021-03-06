// ライセンス: GPL2

//#define _DEBUG
#include "jddebug.h"

#include "tabswitchbutton.h"
#if !GTKMM_CHECK_VERSION(3,0,0)
#include "dragnote.h"
#endif

using namespace SKELETON;


TabSwitchButton::TabSwitchButton( DragableNoteBook* parent )
    : Gtk::Notebook()
#if !GTKMM_CHECK_VERSION(3,0,0)
    , m_parent( parent )
#endif
{
    set_border_width( 0 );

    m_button.add( m_arrow );
    m_button.show_all_children();
    m_button.set_relief( Gtk::RELIEF_NONE );
    m_button.set_focus_on_click( false );

    // フォーカス時にボタンの枠がはみ出さないようにする
#if GTKMM_CHECK_VERSION(3,0,0)
    static_cast< void >( parent );
    m_button.set_margin_top( 0 );
    m_button.set_margin_bottom( 0 );
#else
    Glib::RefPtr< Gtk::RcStyle > rcst = m_button.get_modifier_style();
    rcst->set_ythickness( 0 );
    m_button.modify_style( rcst );
#endif // GTKMM_CHECK_VERSION(3,0,0)

    m_vbox.pack_start( m_button, Gtk::PACK_SHRINK );

    set_show_tabs( false );
}


TabSwitchButton::~TabSwitchButton() noexcept
{}


void TabSwitchButton::show_button()
{
    if( m_shown ) return;
    append_page( m_vbox );
    show_all_children();
    m_shown = true;
}


void TabSwitchButton::hide_button()
{
    if( ! m_shown ) return;
    remove_page( m_vbox );
    m_shown = false;
}


//
// 描画イベント
//
// 自前でビュー領域の枠を描画する
//
#if !GTKMM_CHECK_VERSION(3,0,0)
bool TabSwitchButton::on_expose_event( GdkEventExpose* event )
{
    if( ! m_shown ) return Gtk::Notebook::on_expose_event( event );

    // 枠描画
    m_parent->draw_box( this, event );

    // ボタン描画
    propagate_expose( m_vbox, event );

    return true;
}
#endif // !GTKMM_CHECK_VERSION(3,0,0)
