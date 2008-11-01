// ライセンス: GPL2

//#define _DEBUG
#include "jddebug.h"

#include "mouseconfig.h"
#include "defaultconf.h"

#include "jdlib/miscutil.h"
#include "jdlib/confloader.h"

#include "cache.h"


using namespace CONTROL;


MouseConfig::MouseConfig()
    : MouseKeyConf()
{}



MouseConfig::~MouseConfig()
{}


//
// 設定ファイル読み込み
//
void MouseConfig::load_conf()
{
    std::string str_motions;
    JDLIB::ConfLoader cf( CACHE::path_mouseconf(), std::string() );

    // 共通
    SETMOTION( "Right", MOUSECONF_Right );
    SETMOTION( "Left", MOUSECONF_Left );
    SETMOTION( "TabRight", MOUSECONF_TabRight );
    SETMOTION( "TabLeft", MOUSECONF_TabLeft );
    SETMOTION( "ToggleArticle", MOUSECONF_ToggleArticle );
    SETMOTION( "Home", MOUSECONF_Home );
    SETMOTION( "End", MOUSECONF_End );
    SETMOTION( "Quit", MOUSECONF_Quit );
    SETMOTION( "Reload", MOUSECONF_Reload );
    SETMOTION( "StopLoading", MOUSECONF_StopLoading );
    SETMOTION( "NewArticle", MOUSECONF_NewArticle );
    SETMOTION( "WriteMessage", MOUSECONF_WriteMessage );

    // 全お気に入り更新チェック
    SETMOTION( "CheckUpdateRoot", MOUSECONF_CheckUpdateRoot );
    SETMOTION( "CheckUpdateOpenRoot", MOUSECONF_CheckUpdateOpenRoot );

    // ARTICLE
    SETMOTION( "GotoNew", MOUSECONF_GotoNew );

    // IMAGE
    SETMOTION( "CancelMosaicButton", MOUSECONF_CancelMosaicButton );
}



// ひとつの操作をデータベースに登録
void MouseConfig::set_one_motion_impl( const int id, const int mode, const std::string& name, const std::string& str_motion )
{
    if( name.empty() || str_motion.empty() ) return;

#ifdef _DEBUG
    std::cout << "MouseConfig::set_one_motion_impl " << name << std::endl;
    std::cout << "motion = " << str_motion << std::endl;
    std::cout << CONTROL::get_label( id  ) << std::endl;
#endif

    const bool ctrl = false;
    const bool shift = false;
    const bool alt = false;
    const guint motion = atoi( str_motion.c_str() );
    const bool dblclick = false;
    const bool trpclick = false;

    vec_items().push_back( MouseKeyItem( id, mode, name, str_motion, motion, ctrl, shift, alt, dblclick, trpclick ) );
}


// 操作文字列取得
const std::string MouseConfig::get_str_motions( const int id_ )
{
    int id = id_;

    // (注) この行が無いと画像ビューのコンテキストメニューにマウスジェスチャが表示されない
    if( id == CONTROL::CancelMosaic ) id = CONTROL::CancelMosaicButton;

    return MouseKeyConf::get_str_motions( id );
}


// IDからデフォルトの操作文字列取得
const std::string MouseConfig::get_default_motions( const int id_ )
{
    int id = id_;
    if( id == CONTROL::CancelMosaic ) id = CONTROL::CancelMosaicButton;

    return MouseKeyConf::get_default_motions( id );
}