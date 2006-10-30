// ライセンス: 最新のGPL

//
// 2ch 
//

#ifndef _BOARD2CH_H
#define _BOARD2CH_H

#include "board2chcompati.h"

namespace DBTREE
{
    class Board2ch : public Board2chCompati
    {
      public:

        Board2ch( const std::string& root, const std::string& path_board,const std::string& name );
        virtual ~Board2ch();

        virtual const std::string& get_agent();

        virtual const std::string get_proxy_host();
        virtual const int get_proxy_port();

        virtual const std::string get_proxy_host_w();
        virtual const int get_proxy_port_w();

        // 新スレ作成用のメッセージ変換
        virtual const std::string create_newarticle_message( const std::string& subject,
                                                             const std::string& name, const std::string& mail, const std::string& msg );

      private:

        virtual ArticleBase* append_article( const std::string& id, bool cached );
    };
}

#endif
