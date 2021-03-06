#include "body.h"
namespace smoothly{

void body::loop(){
    terrainDispather::loop();
    for(auto it:bodies){
        bodyItem * b = it.second;
        auto p = b->node->getPosition();
        auto cp=camera->getPosition();
        int l = std::max(abs((p.X-cp.X)/32),abs((p.Z-cp.Z)/32));
        if(l<getVisualRange()){
            if(mainControlBody==b){
                b->node->setVisible(false);
            }else{
                b->node->setVisible(true);
            }
        }else{
            b->node->setVisible(false);
        }
        if(b->uncreatedChunk){
            int cx = floor(b->lastPosition.X/32);
            int cy = floor(b->lastPosition.Z/32);
            if(chunkCreated(cx,cy)){
                b->m_character.setGravity(gravity);
                b->uncreatedChunk = false;
            }else{
                b->m_character.setPosition(b->lastPosition);
                b->updateFromWorld();
                continue;
            }
        }
        b->walk(b->status.walk_forward , b->status.walk_leftOrRight,b->config->walkVelocity);
        b->updateFromWorld();
        auto h = getRealHight(p.X,p.Z);
        if(p.Y<h){
            if(b->lastPosition.Y>=h){
                p.Y = b->lastPosition.Y;
            }else{
                p.Y = (b->config->height+b->config->width)*0.5+h;//防止掉出地图
            }
            b->m_character.setPosition(p);
        }
    }
    for(auto it:myBodies){
        bodyItem * b = it.second;
        if(b->owner == myUUID && (!myUUID.empty())){//是自己拥有的

            //设置行走
            //b->walk(b->status.walk_forward , b->status.walk_leftOrRight,b->config->walkVelocity);
            //转移到上面了
            //b->walk(b->status.walk_forward , b->status.walk_leftOrRight,b->config->walkVelocity*timer->getRealTime());

            //设置dispather
            btTransform transform;
            b->m_character.getTransform(transform);

            btVector3 btPos;
            btPos = transform.getOrigin();
            setCharacterChunk(b->uuid,floor(btPos.x()/32),floor(btPos.z()/32));

            if(b->firing)
                b->doFire();

            //ai控制
            if(mainControlBody!=b){
                commond cmd;//命令结构体
                cmd.uuid = b->uuid;
                vec3 posi(btPos.x(), btPos.y(), btPos.z());
                if(b->follow != NULL && b->autoWalk.empty()){
                    //跟随模式
                    bodyItem * bd = b->follow;
                    auto target  = bd->node->getPosition();

                    irr::core::vector2df a(posi.X,posi.Z),b(target.X,target.Z);

                    if((a-b).getLengthSQ()>5*5){
                        vec3 dir = target-posi;
                        //行走
                        cmd.data_int = BM_WALK_F;
                        cmd.cmd = CMD_STATUS_ADD;
                        pushCommond(cmd);

                        //更新旋转
                        cmd.cmd = CMD_SET_LOOKAT;
                        cmd.data_vec = dir;
                        pushCommond(cmd);

                        if(dir.Y>2){//目标点高于现在，跳跃
                            cmd.cmd = CMD_JUMP;
                            cmd.data_vec.set(vec3(0,1,0));
                            pushCommond(cmd);
                        }
                    }else{
                        cmd.data_int = BM_WALK_F|BM_WALK_B|BM_WALK_L|BM_WALK_R;
                        cmd.cmd = CMD_STATUS_REMOVE;
                        pushCommond(cmd);
                    }

                }else{
                    while(!b->autoWalk.empty()){
                        auto target = b->autoWalk.front();
                        vec3 dir = target-posi;

                        irr::core::vector2df tdir(dir.X,dir.Z);
                        if(tdir.getLengthSQ()<1){
                            b->autoWalk.pop_front();//到达目标，删除任务
                        }else{

                            //行走
                            cmd.data_int = BM_WALK_F;
                            cmd.cmd = CMD_STATUS_ADD;
                            pushCommond(cmd);

                            //更新旋转
                            cmd.cmd = CMD_SET_LOOKAT;
                            cmd.data_vec = dir;
                            pushCommond(cmd);

                            if(dir.Y>2){//目标点高于现在，跳跃
                                cmd.cmd = CMD_JUMP;
                                cmd.data_vec.set(vec3(0,1,0));
                                pushCommond(cmd);
                            }
                            break;
                        }
                    }
                    if(b->autoWalk.empty()){//停止自动行走
                        cmd.data_int = BM_WALK_F|BM_WALK_B|BM_WALK_L|BM_WALK_R;
                        cmd.cmd = CMD_STATUS_REMOVE;
                        pushCommond(cmd);
                    }
                }
            }
        }
    }
    doCommonds();

    selectBodyUpdate();
}

void body::onDraw(){
    terrainDispather::onDraw();
    for(auto it:myBodies){//在屏幕上标出自己拥有的单位
        bodyItem * bd = it.second;
        if(bd!=mainControlBody){
            auto p = scene->getSceneCollisionManager()->getScreenCoordinatesFrom3DPosition(bd->node->getPosition(),camera);
            bd->screenPosition = p;
            if(p.X<0 || p.Y<0 || p.X>width || p.Y>height)
                continue;
            driver->draw2DLine(irr::core::vector2d<irr::s32>(p.X-2,p.Y),irr::core::vector2d<irr::s32>(p.X+2,p.Y),irr::video::SColor(255,64,255,255));
            driver->draw2DLine(irr::core::vector2d<irr::s32>(p.X-2,p.Y),irr::core::vector2d<irr::s32>(p.X,p.Y+5),irr::video::SColor(255,64,255,255));
            driver->draw2DLine(irr::core::vector2d<irr::s32>(p.X+2,p.Y),irr::core::vector2d<irr::s32>(p.X,p.Y+5),irr::video::SColor(255,64,255,255));
            if(!bd->autoWalk.empty()){
            #define dig120 (3.14159265358979323846*2)/3
            #define inr    19
            #define our    25
                double t=(std::clock()*500.0/ CLOCKS_PER_SEC)/100.0;
                driver->draw2DLine(
                            irr::core::vector2d<irr::s32>(p.X + (inr*cos(-t)), p.Y + (inr*sin(-t))),
                            irr::core::vector2d<irr::s32>(p.X + (our*cos(-t)), p.Y + (our*sin(-t))),
                            irr::video::SColor(255,255,255,255));
                driver->draw2DLine(
                            irr::core::vector2d<irr::s32>(p.X + (inr*cos(-t+dig120)), p.Y + (inr*sin(-t+dig120))),
                            irr::core::vector2d<irr::s32>(p.X + (our*cos(-t+dig120)), p.Y + (our*sin(-t+dig120))),
                            irr::video::SColor(255,255,255,255));
                driver->draw2DLine(
                            irr::core::vector2d<irr::s32>(p.X + (inr*cos(-t-dig120)), p.Y + (inr*sin(-t-dig120))),
                            irr::core::vector2d<irr::s32>(p.X + (our*cos(-t-dig120)), p.Y + (our*sin(-t-dig120))),
                            irr::video::SColor(255,255,255,255));
            #undef our
            #undef inr
            #undef dig120
            }
        }
    }
    for(auto it:selectedBodies){
        bodyItem * bd = it;
        auto p = bd->screenPosition;
        if(bd==mainControlBody)
            continue;
        if(p.X<0 || p.Y<0 || p.X>width || p.Y>height)
            continue;

        #define dig120 (3.14159265358979323846*2)/3
        #define inr    7
        #define our    19
        double t=(std::clock()*500.0/ CLOCKS_PER_SEC)/100.0;
        driver->draw2DLine(
                    irr::core::vector2d<irr::s32>(p.X + (inr*cos(t)), p.Y + (inr*sin(t))),
                    irr::core::vector2d<irr::s32>(p.X + (our*cos(t)), p.Y + (our*sin(t))),
                    irr::video::SColor(255,255,255,64));
        driver->draw2DLine(
                    irr::core::vector2d<irr::s32>(p.X + (inr*cos(t+dig120)), p.Y + (inr*sin(t+dig120))),
                    irr::core::vector2d<irr::s32>(p.X + (our*cos(t+dig120)), p.Y + (our*sin(t+dig120))),
                    irr::video::SColor(255,255,255,64));
        driver->draw2DLine(
                    irr::core::vector2d<irr::s32>(p.X + (inr*cos(t-dig120)), p.Y + (inr*sin(t-dig120))),
                    irr::core::vector2d<irr::s32>(p.X + (our*cos(t-dig120)), p.Y + (our*sin(t-dig120))),
                    irr::video::SColor(255,255,255,64));
        #undef our
        #undef inr
        #undef dig120
    }

    if(selecting){
        driver->draw2DPolygon(screenCenter,selectBodyRange,irr::video::SColor(255,255,255,255),36);
    }
}

}
