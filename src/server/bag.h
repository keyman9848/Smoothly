﻿#ifndef SMOOTHLY_SERVER_BAG
#define SMOOTHLY_SERVER_BAG
#include "body.h"
#include "../utils/uuid.h"
#include "../utils/cJSON.h"
#include <unordered_set>

#include <raknet/RakPeerInterface.h>

namespace smoothly{
namespace server{

class bag:public body{
    public:
        ////////////////////////////////////////////////////////////
        struct bag_tool_conf{//工具
                int  id;
                int  weight;    //重量
                int  durability;//耐久度
        };
        std::map<int,bag_tool_conf*> tool_config;
        ////////////////////////////////////////////////////////////
        struct bag_resource_conf{//资源
                int id;
                int weight;
        };
        std::map<int,bag_resource_conf*> resource_config;
        ////////////////////////////////////////////////////////////
        struct bag_tool{
                bag_tool_conf * conf;
                int durability;
                std::string inbag;
                bag * parent;
                void toString(std::string & str);
                void loadString(const std::string & str);//使用前请先设置parent

                inline bag_tool(){
                    conf        = NULL;
                    durability  = 0;
                    inbag.clear();
                    parent      = NULL;
                }
                inline bag_tool(const bag_tool & i){
                    conf        = i.conf;
                    durability  = i.durability;
                    inbag       = i.inbag;
                    parent      = i.parent;
                }
                inline const bag_tool & operator=(const bag_tool & i){
                    conf        = i.conf;
                    durability  = i.durability;
                    inbag       = i.inbag;
                    parent      = i.parent;
                    return * this;
                }
        };
        class cache_tools_t:public cache<bag_tool>{//用cache存放
                void onExpire(const std::string &,bag_tool & )override;
                void onLoad(const std::string &, bag_tool & )override;
            public:
                bag * parent;
        }cache_tools;
        std::string createTool(int id);//创建一个新的工具
        ////////////////////////////////////////////////////////////
        struct bag_inner{
                std::map<int,int>                           resources;  //基础资源，弹药（id=>数量）
                std::unordered_set<std::string>             tools;      //工具（存放uuid）
                int weight,maxWeight;
                bag * parent;
                std::string                                 uuid;       //自己的uuid(load的时候不会被设置，需要手动设置)

                inline bag_inner(){
                    resources.clear();
                    tools.clear();
                    weight = 0;
                    maxWeight = 0;
                    parent = NULL;
                    uuid.clear();
                }
                inline bag_inner(const bag_inner & i){
                    resources   =   i.resources;
                    tools       =   i.tools;
                    weight  =   i.weight;
                    maxWeight = i.maxWeight;
                    parent      =   i.parent;
                    uuid        =   i.uuid;
                }
                inline const bag_inner & operator=(const bag_inner & i){
                    resources   =   i.resources;
                    tools       =   i.tools;
                    weight  =   i.weight;
                    maxWeight = i.maxWeight;
                    parent      =   i.parent;
                    uuid        =   i.uuid;
                    return * this;
                }

                //这三个记得处理好throw
                bool addResource(int id,int num);
                void addTool(const std::string & uuid);
                void removeTool(const std::string & uuid);

                void toString(std::string & str);
                void loadString(const std::string & str);
        };
        class cache_bag_inner_t:public cache<bag_inner>{//用cache存放
                void onExpire(const std::string &,bag_inner & )override;
                void onLoad(const std::string &, bag_inner & )override;//bag找不到会自动创建，config找不到会throw
            public:
                bag * parent;
        }cache_bag_inner;
        std::map<int,int> maxWeights;

        bool consume(const std::string & bag_uuid,const std::string & tool_uuid,int num)noexcept;//消耗耐久
        bool consume(const std::string & tool_uuid,int num)noexcept;//消耗耐久

        bag();
        ~bag();

        void sendBagToAddr(const RakNet::SystemAddress & addr, const std::string & uuid);
        bool addResource(const RakNet::SystemAddress & addr, const std::string & uuid,int id,int delta);
        virtual void sendAddr_bag(const RakNet::SystemAddress & addr,const std::string & uuid,const std::string & text)=0;
        virtual void sendAddr_bag_resourceNum(const RakNet::SystemAddress & addr,const std::string & uuid,int id,int num)=0;

        void release()override;
        void loop()override;
};

}
}
#endif // SMOOTHLY_SERVER_BAG
