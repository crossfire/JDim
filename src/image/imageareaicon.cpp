// ライセンス: GPL2

//#define _DEBUG
#include "jddebug.h"

#include "imageareaicon.h"

#include "dbimg/img.h"

#include "jdlib/miscmsg.h"

#include "cache.h"
#include "global.h"
#include "httpcode.h"


//
// スレッドのランチャ
//
// icon_launcherの引数で直接 ImageAreaIcon のポインタを渡すと
// スレッドが起動する前に ImageAreaIcon が delete されると落ちるのでリストを使う

Glib::StaticMutex icon_launcher_mutex = GLIBMM_STATIC_MUTEX_INIT;
std::list< IMAGE::ImageAreaIcon* > icon_launcher_list_icon;

void icon_launcher_set( IMAGE::ImageAreaIcon* icon ){
    Glib::Mutex::Lock lock( icon_launcher_mutex);
    icon_launcher_list_icon.push_back( icon );
}

void icon_launcher_remove( IMAGE::ImageAreaIcon* icon ){
    Glib::Mutex::Lock lock( icon_launcher_mutex);
    icon_launcher_list_icon.remove( icon );
}

void* icon_launcher( void* )
{
#ifdef _DEBUG
    std::cout << "start icon_launcher\n";
#endif

    if( ! icon_launcher_list_icon.size() ) return 0;

    IMAGE::ImageAreaIcon* icon = *( icon_launcher_list_icon.begin() );
    icon_launcher_list_icon.remove( icon );
    icon->show_image_thread();

    return 0;
}



//////////////////////////////////

using namespace IMAGE;

ImageAreaIcon::ImageAreaIcon( const std::string& url )
    : ImageAreaBase( url )
    , m_thread( 0 )
    , m_shown( false )
{
#ifdef _DEBUG    
    std::cout << "ImageAreaIcon::ImageAreaIcon url = " << url << std::endl;
#endif 

    set_width( ICON_SIZE );
    set_height( ICON_SIZE );

    if( ! get_img()->is_cached() ) show_indicator( true );
    else show_image();
}



ImageAreaIcon::~ImageAreaIcon()
{
#ifdef _DEBUG    
    std::cout << "ImageAreaIcon::~ImageAreaIcon url = " << get_url() << std::endl;
#endif 

    // デストラクタの中からdispatchを呼ぶと落ちるので dispatch不可にする
    set_dispatchable( false );

    icon_launcher_remove( this );
    wait();
}


void ImageAreaIcon::wait()
{
#ifdef _DEBUG    
    std::cout << "ImageAreaIcon::wait\n";
#endif 

    if( m_thread ) pthread_join( m_thread, NULL );
    m_thread = 0;
}


//
// 表示
//
// メインスレッドで大きい画像を開くと反応が無くなるので別の
// スレッドを起動して開く。
//
void ImageAreaIcon::show_image()
{
    wait();

    // 既に画像が表示されている
    if( m_shown && get_img()->is_cached() ) return;

#ifdef _DEBUG    
    std::cout << "ImageAreaIcon::show_image url = " << get_url() << std::endl;
#endif 

    if( m_pixbuf ){
        m_pixbuf.clear();
        m_pixbuf_loading.clear();
        m_pixbuf_err.clear();
    }
    if( m_pixbuf_icon ) m_pixbuf_icon.clear();

    m_shown = false;
    set_ready( false );

    icon_launcher_set( this );
    int status;
    if( ( status = pthread_create( &m_thread, NULL, icon_launcher, NULL ) ) ){
        MISC::ERRMSG( "ImageAreaIcon::show_image : could not start thread" );
        icon_launcher_remove( this );
    }
}


//
// 表示スレッド
//
void ImageAreaIcon::show_image_thread()
{
#ifdef _DEBUG
    std::cout << "ImageAreaIcon::show_image_thread url = " << get_url() << std::endl;
#endif

    // キャッシュされてない時は読み込みorエラーマークを表示
    if( ! get_img()->is_cached() ){

        show_indicator( ( get_img()->is_loading() || get_img()->get_code() == HTTP_INIT ) );
    }

    // 画像を読み込んで縮小表示
    else{

        try{

            // 画像ロード
            std::string path_cache = CACHE::path_img( get_url() );
            Glib::RefPtr< Gdk::Pixbuf > pixbuf;
            pixbuf = Gdk::Pixbuf::create_from_file( path_cache );
            set_width_org( pixbuf->get_width() );
            set_height_org( pixbuf->get_height() );

            // 縮小比率を計算して縮小
            
            double scale;
            double scale_w = ( double ) ICON_SIZE / get_width_org();
            double scale_h = ( double ) ICON_SIZE / get_height_org();
            scale = MIN( scale_w, scale_h );
            set_width( (int)( get_width_org() * scale ) );
            set_height( (int)( get_height_org() * scale ) );
            m_pixbuf_icon = pixbuf->scale_simple( get_width(), get_height(), Gdk::INTERP_NEAREST );

            // 表示
            // ディスパッチャ経由で callback_dispatch() -> set_image() と呼び出される
            m_imagetype = IMAGE_SHOW_ICON;
            dispatch();

            m_shown = true;
        }
        catch( Glib::Error& err )
        {
            set_errmsg( err.what() );
            MISC::ERRMSG( get_errmsg() );

            show_indicator( false );
        }
    }

#ifdef _DEBUG
    std::cout << "ImageAreaIcon::show_image_thread finished\n";
#endif    
}


//
// インジゲータ画像表示
// 
void ImageAreaIcon::show_indicator( bool loading )
{
#ifdef _DEBUG
        std::cout << "ImageAreaIcon::show_indicato load = "  << loading << std::endl;
#endif        

    if( ! m_pixbuf ){

        m_pixbuf = Gdk::Pixbuf::create( Gdk::COLORSPACE_RGB, false, 8, get_width(), get_height() );
        m_pixbuf_err = Gdk::Pixbuf::create( Gdk::COLORSPACE_RGB, false, 8, get_width()/4, get_height()/4 );
        m_pixbuf_loading = Gdk::Pixbuf::create( Gdk::COLORSPACE_RGB, false, 8, get_width()/4, get_height()/4 );

        assert( m_pixbuf );
        assert( m_pixbuf_err );
        assert( m_pixbuf_loading );

        m_pixbuf->fill( 0xffffff00 );
        m_pixbuf_loading->fill( 0xffbf0000 );
        m_pixbuf_err->fill( 0xff000000 );
    }

    // 読み込み中
    if( loading ) m_pixbuf_loading->copy_area( 0, 0, get_width()/4, get_height()/4, m_pixbuf, 4, 4 );

    // エラー
    else  m_pixbuf_err->copy_area( 0, 0, get_width()/4, get_height()/4, m_pixbuf, 4, 4 );

    // 表示
    // ディスパッチャ経由で callback_dispatch() -> set_image() と呼び出される
    m_imagetype = IMAGE_SHOW_INDICATOR;
    dispatch();
}



//
// ディスパッチャのコールバック関数
//
void ImageAreaIcon::callback_dispatch()
{
    set_image();
    wait();
}



//
// 表示
//
// スレッドの中でset()すると固まるときがあるのでディスパッチャで
// メインスレッドに戻してからセットする
//
void ImageAreaIcon::set_image()
{
#ifdef _DEBUG
    std::cout << "ImageAreaIcon::set_image type = " << m_imagetype << std::endl;
#endif    

    clear();
    if( m_imagetype == IMAGE_SHOW_ICON ){

#ifdef _DEBUG
    std::cout << "show icon\n";
#endif    

        set( m_pixbuf_icon );
    }

    else if( m_imagetype == IMAGE_SHOW_INDICATOR ){

#ifdef _DEBUG
    std::cout << "show indicator\n";
#endif    

        set( m_pixbuf );
    }

    set_ready( true );
}
