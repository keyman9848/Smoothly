#include "terrain.h"
#include "mempool.h"
#include <stdio.h>
#include <set>
#include <mutex>
namespace smoothly{

typedef mempool<terrain::chunk> chunkpool;
typedef mempool<terrain::item> itempool;
void terrain::createUpdatePath(int range){
    updatePath.clear();
    updatePath.push_back(ipair(0,0));
    /*
     * Ab------------------B
     * |                   c
     * |                   |
     * |                   |
     * |                   |
     * |         0         |
     * |                   |
     * |                   |
     * |                   |
     * a                   |
     * D------------------dC
    */ 
    for(int i=1;i<range;++i){
        {//a
            int a=-i+1;
            int A=i;
            int ax=-i;
            for(int j=a;j<=A;j++)
                updatePath.push_back(ipair(ax,j));
        }
        {//b
            int b=-i+1;
            int B=i;
            int by=i;
            for(int j=b;j<=B;j++)
                updatePath.push_back(ipair(j,by));
        }
        {//c
            int c=i-1;
            int C=-i;
            int cx=i;
            for(int j=c;j>=C;j--)
                updatePath.push_back(ipair(cx,j));
        }
        {//d
            int d=i-1;
            int D=-i;
            int dy=-i;
            for(int j=d;j>=D;j--)
                updatePath.push_back(ipair(j,dy));
        }
    }
}
terrain::terrain(){
    this->cpool=new chunkpool;
    this->ipool=new itempool;
    this->texture=NULL;
    pointNum=34;
    altitudeK=0.08;
    hillK=100;
    temperatureK=0.3;
    humidityK=0.3;
    altitudeArg=20000;
    hillArg=500;
    temperatureArg=2000;
    humidityArg=2000;
    
    temperatureMax=2000;
    humidityMax=2000;
    
    temperatureMin=0;
    humidityMin=0;
    
    scene =NULL;
    createUpdatePath(16);
}

terrain::~terrain(){
    delete (chunkpool*)this->cpool;
    delete (itempool*)this->ipool;
}
void terrain::destroy(){
    chunkmtx.lock();
    for(auto it:chunks){
        if(it.second){
            it.second->remove();
            removeChunk(it.second);
        }
    }
    chunks.clear();
    chunkmtx.unlock();
}

terrain::item * terrain::createItem(){
    auto ptr=((itempool*)this->ipool)->get();
    
    return ptr;
}

void terrain::destroyItem(terrain::item * ptr){
    ((itempool*)this->ipool)->del(ptr);
}
void terrain::genRiver(int seed){
    predictableRand randg;
    randg.setSeed(seed);
    for(int i=0;i<3;i++){
        rivers[i].a=(randg.rand())%1024-512;
        rivers[i].b=(randg.rand())%1024-512;
        rivers[i].c=(randg.rand())%1024-512;
        rivers[i].d=(randg.rand())%1024-512;
        rivers[i].e=(randg.rand())%1024-512;
        rivers[i].f=(randg.rand())%1024-512;
        rivers[i].g=(randg.rand())%1024-512;
        rivers[i].h=(randg.rand())%1024-512;
        rivers[i].i=(randg.rand())%1024-512;
        rivers[i].j=(randg.rand())%1024-512;
        rivers[i].w=(randg.rand())%8+2;
    }
}
float terrain::getRiver(float x , float y){
    float K=1;
    for(int i=0;i<3;i++){
        K*=rivers[i].getK(x,y);
    }
    return K;
}
float terrain::river::sigmoid(float x){
    return (1/(1+exp(-x)));
}
float terrain::river::getK(float x,float y){
    float de=fabs(getD(x,y));
    float s=sigmoid(de/w-4);
    return s;
}
float terrain::river::getD(float x,float y){
    float x2=x *x;
    float x4=x2*x2;
    float x8=x4*x4;
    
    float y2=y *y;
    float y4=y2*y2;
    float y8=y4*y4;
    
    return (
        x8*a + y8*b + 
        x4*c + y4*d + 
        x2*e + y2*f +
        x *g + y *h +
           i +    j
    );
}
terrain::chunk * terrain::createChunk(){
    auto ptr=((chunkpool*)this->cpool)->get();
    ptr->scene=this->scene;
    ptr->device=this->device;
    ptr->mesh=NULL;
    ptr->node=NULL;
    ptr->parent=this;
    ptr->generator=&(this->generator);
    ptr->items.clear();
    ptr->removeTable.clear();
    return ptr;
}

void terrain::removeChunk(terrain::chunk * ptr){
    //for(auto it:ptr->items){
    //    if(it)
    //        remove(it);
    //}
    ptr->items.clear();
    ptr->removeTable.clear();
    ((chunkpool*)this->cpool)->del(ptr);
}

void terrain::visualChunkUpdate(irr::s32 x , irr::s32 y , bool force){
    if(x==px && y==py && !force)
        return;
    int i,j;
    px=x;
    py=y;
    std::map<ipair,chunk*> rm;
    
    chunkmtx.lock();
    for(auto it : chunks){
        rm[it.first]=it.second;
    }
    chunkmtx.unlock();
    
    for(auto it:updatePath){
        int ix=x+it.x;
        int iy=y+it.y;
        ipair posi(ix , iy);
        chunkmtx.lock();
        auto it2=chunks.find(posi);
        if(it2!=chunks.end()){
            rm.erase(posi);
        }else{
            auto ptr=createChunk();
            ptr->x=ix;
            ptr->y=iy;
            //updateChunk(ptr , ix , iy);
            ptr->nodeInited=false;
            ptr->rigidBody  =NULL;
            ptr->bodyShape  =NULL;
            ptr->bodyState  =NULL;
            ptr->bodyMesh   =NULL;
            ptr->mesh       =NULL;
            sendTChunkQL.lock();
            sendTChunkQ.push(std::pair<chunk*,tuMethod>(ptr,TU_CREATE));
            sendTChunkQL.unlock();
            chunks[posi]=ptr;
            terrainWake();
            //chunks[posi]=NULL;
            //requestUpdateChunk(ix,iy);
        }
        chunkmtx.unlock();
    }
    
    for(auto it3:rm){
        //this->onFreeChunk(it3.second);
        //it3.second->remove();
        //removeChunk(it3.second);
        sendTChunkQL.lock();
        sendTChunkQ.push(std::pair<chunk*,tuMethod>(it3.second,TU_DELETE));
        sendTChunkQL.unlock();
        chunkmtx.lock();
        chunks.erase(it3.first);
        chunkmtx.unlock();
    }
}

float terrain::genTerrain(
    float ** img,  //高度图边长=chunk边长+1
    irr::s32 x , irr::s32 y //chunk坐标，真实坐标/32
){
    float h;
    float ix,iy;
    float max=0;
    float mx=0,my=0;
    float begX=x*32;
    float begY=y*32;
    float len=33.0f/((float)pointNum);
    
    for(int i=0;i<pointNum;i++){
        for(int j=0;j<pointNum;j++){
            //ix=len*(pointNum-i+1)+begX;
            ix=len*i+begX;
            //iy=len*(pointNum-j+1)+begY;
            iy=len*j+begY;
            h=getRealHight(ix,iy);
            if(h>max){
                max=h;
                mx=ix;
                my=iy;
            }
            img[i][j]=h;
        }
    }
    //printf("max:(%f,%f) begin:(%d,%d)\n",mx,my,begX,begY);
    return max;
}

float terrain::chunk::getRealHight(float x,float y){
    return parent->getRealHight(x,y);
}

void terrain::remove(item * i){
    onFreeTerrainItem(i);
    if(i->inChunk)
        allItems.erase(mapid(i->inChunk->x , i->inChunk->y , i->id , i->mapId));
    //i->inChunk->items.erase(i);
    i->remove();
    destroyItem(i);
}

bool terrain::remove(const mapid & mid){
    auto it=allItems.find(mid);
    if(it==allItems.end()){
        return false;
    }
    if(it->second==NULL)
        return false;
    
    onFreeTerrainItem(it->second);
    it->second->inChunk->items.erase(it->second);
    it->second->remove();
    
    //requestRemoveItem(mid);
    addIntoRemoveTable(mid);
    
    destroyItem(it->second);
    
    allItems.erase(it);
    return true;
}

int terrain::chunk::add(
    long id,
    const irr::core::vector3df & p,
    const irr::core::vector3df & r,
    const irr::core::vector3df & s
){
    auto mit=parent->m->items.find(id);
    if(mit==parent->m->items.end()){
        return -1;
    }
    int mapId=this->getId(id);
    //printf("add id=%d (%f,%f,%f) mapId=%d\n" , id , p.X , p.Y , p.Z , mapId);
    //set ptr
    //if(!parent->itemExist(x,y,id,mapId))
    //    return -1;
    
    auto rit=removeTable.find(id);
    if(rit!=removeTable.end()){
        if(rit->second.find(mapId)!=rit->second.end())
            return -1;
    }
    
    if(parent->allItems.find(mapid(x,y,id,mapId))!=parent->allItems.end())
        return -2;
    
    char buf[256];
    auto ptr=parent->createItem();
    ptr->parent=mit->second;
    ptr->mesh=mit->second->mesh;
    ptr->inChunk=this;
    ptr->id=id;
    ptr->mapId=mapId;
    //set node
    ptr->node=parent->scene->addMeshSceneNode(
        ptr->mesh,
        0,-1,p,r,s
    );
    ptr->node->setMaterialFlag(irr::video::EMF_LIGHTING, true );
    parent->getItemName(buf,sizeof(buf),x,y,id,mapId);
    ptr->node->setName(buf);
    
    if(mit->second->bodyShape){
        ptr->bodyState=setMotionState(ptr->node->getAbsoluteTransformation().pointer());
        ptr->rigidBody=createBody(mit->second->bodyShape,ptr->bodyState);
        ptr->rigidBody->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);
        parent->dynamicsWorld->addRigidBody(ptr->rigidBody);
    }else
        ptr->rigidBody=NULL;
    //set index
    this->items.insert(ptr);
    parent->allItems[mapid(x,y,id,mapId)]=ptr;
    parent->onGenTerrainItem(ptr);
    ///////
    return ptr->mapId;
}

void terrain::setRemoveTable(int x,int y,const std::list<std::pair<long,int> > & t){
    chunkmtx.lock();
    auto it=chunks.find(ipair(x,y));
    if(it==chunks.end()){
        chunkmtx.unlock();
        return;
    }
    
    auto ch=it->second;
    
    if(ch==NULL){
        chunkmtx.unlock();
        return;
    }
    
    for(auto it:t){
        ch->removeTable[it.first].insert(it.second);
    }
    chunkmtx.unlock();
}
void terrain::removeTableApply(int x,int y){
    chunkmtx.lock();
    auto it=chunks.find(ipair(x,y));
    if(it==chunks.end()){
        chunkmtx.unlock();
        return;
    }
    auto ch=it->second;
    ch->itemNum.clear();
    ch->autoGen(x,y,getTemperatureLevel(x,y),getHumidityLevel(x,y),getHight(x,y),m);
    for(auto it:m->mapGenFuncs){
        if(it)
            it(x,y,getTemperature(x,y),getHumidity(x,y),getHight(x,y),ch);
    }
    chunkmtx.unlock();
}

void terrain::getItems(irr::s32 x , irr::s32 y , terrain::chunk * ch){
    for(auto it:m->mapGenFuncs){
        if(it)
            it(x,y,getTemperature(x,y),getHumidity(x,y),getHight(x,y),ch);
    }
}
void terrain::terrainParseOne(){
    long t=timer->getRealTime();
    begin:
    if(fabs(timer->getRealTime()-t)>20)
        return;
    recvTChunkQL.lock();
    if(recvTChunkQ.empty()){
        recvTChunkQL.unlock();
        return;
    }
    auto p=recvTChunkQ.front();
    recvTChunkQ.pop();
    recvTChunkQL.unlock();
    if(p.second==TU_DELETE){
        this->onFreeChunk(p.first);
        p.first->remove();
        removeChunk(p.first);
    }else
        updateChunk(p.first);
    goto begin;
}
void terrain::terrainLoop(){
    recvTChunkQL.lock();
    while(!recvTChunkQ.empty()){
        auto p=recvTChunkQ.front();
        recvTChunkQ.pop();
        recvTChunkQL.unlock();
        if(p.second==TU_DELETE){
            this->onFreeChunk(p.first);
            p.first->remove();
            removeChunk(p.first);
        }else
            updateChunk(p.first);
        recvTChunkQL.lock();
    }
    recvTChunkQL.unlock();
}
void terrain::updateTerrainThread(){
    sendTChunkQL.lock();
    while(!sendTChunkQ.empty()){
        auto p=sendTChunkQ.front();
        sendTChunkQ.pop();
        sendTChunkQL.unlock();
        if(p.second==TU_DELETE){
            recvTChunkQL.lock();
            recvTChunkQ.push(std::pair<chunk*,tuMethod>(p.first,TU_DELETE));
            recvTChunkQL.unlock();
        }else
            updateChunkThread(p.first);
        sendTChunkQL.lock();
    }
    sendTChunkQL.unlock();
    std::unique_lock <std::mutex> lck(sqmtx);
    sqcv.wait(lck);
}
void terrain::updateChunkThread(terrain::chunk * ch){
    if(!chunkExist(ch->x ,ch->y))
        return;
    float ** T=new float*[pointNum];
    for(auto i=0;i<pointNum;i++)
        T[i]=new float[pointNum];
    
    genTerrain(T , ch->x ,ch->y);
    float len=33.0f/(float)pointNum;
    ch->mesh=this->createTerrainMesh(
        this->texture ,
        T , pointNum , pointNum ,
        irr::core::dimension2d<irr::f32>(len , len),
        irr::core::dimension2d<irr::u32>(pointNum , pointNum),
        true
    );
    for(auto i=0;i<pointNum;i++)
        delete [] T[i];
    delete [] T;
    recvTChunkQL.lock();
    recvTChunkQ.push(std::pair<chunk*,tuMethod>(ch,TU_CREATE));
    recvTChunkQL.unlock();
}
void terrain::updateChunk(terrain::chunk * ch){
    char buf[128];
    auto driver=this->scene->getVideoDriver();
    
    ch->node=scene->addMeshSceneNode(
        ch->mesh,
        0,-1
    );
    getChunkName(buf,sizeof(buf),ch->x,ch->y);
    ch->node->setName(buf);
    
    ch->selector=scene->createOctreeTriangleSelector(ch->mesh,ch->node);
    ch->node->setTriangleSelector(ch->selector);
    ch->node->setPosition(irr::core::vector3df(ch->x*32.0f , 0 , ch->y*32.0f));
    //printf("position:(%f,%f)(%d,%d)\n",x*32.0f,y*32.0f,x,y);
    ch->node->setMaterialFlag(irr::video::EMF_LIGHTING, true );
    
    ch->bodyMesh  =createBtMesh(ch->mesh);
    ch->bodyShape =createShape(ch->bodyMesh);
    ch->bodyState =setMotionState(ch->node->getAbsoluteTransformation().pointer());
    ch->rigidBody =createBody(ch->bodyShape,ch->bodyState);
    ch->rigidBody->setCollisionFlags(btCollisionObject::CF_STATIC_OBJECT);
    this->dynamicsWorld->addRigidBody(ch->rigidBody);
    
    ch->itemNum.clear();
    //getItems(x,y,ch);
    requestUpdateTerrain(ch->x,ch->y);
    this->onGenChunk(ch);
    ch->nodeInited=true;
    
}

void terrain::genTexture(){
    if(this->texture)
        return;
    printf("generate texture\n");
    auto driver=this->scene->getVideoDriver();
    this->texture=driver->getTexture("./res/terrain/grass.jpg");
    //this->texture=driver->addTexture("terrain-grass", img);
    //img->drop();
}

void terrain::destroyTexture(){
    this->texture->drop();
    this->texture=NULL;
}


bool terrain::selectPointM(
    float x,float y,
    const irr::core::line3d<irr::f32>& ray,
    irr::core::vector3df& outCollisionPoint,
    irr::core::triangle3df& outTriangle,
    irr::scene::ISceneNode*& outNode
){
    int cx=(int)(x/32);
    int cy=(int)(y/32);
    float ipx=x-cx*32;
    float ipy=y-cy*32;
    if(selectPointInChunk(cx,cy,ray,outCollisionPoint,outTriangle,outNode))
        return true;
    if(ipx>16){
        if(ipy>16){
            //(cx+1,cy+1)
            if(selectPointInChunk(cx+1,cy,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
            if(selectPointInChunk(cx+1,cy+1,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
            if(selectPointInChunk(cx  ,cy+1,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
                return false;
        }else{
            //(cx+1,cy-1)
            if(selectPointInChunk(cx+1,cy,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
            if(selectPointInChunk(cx+1,cy-1,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
            if(selectPointInChunk(cx  ,cy-1,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
                return false;
        }
    }else{
        if(ipy>16){
            //(cx-1,cy+1)
            if(selectPointInChunk(cx-1,cy,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
            if(selectPointInChunk(cx-1,cy+1,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
            if(selectPointInChunk(cx  ,cy+1,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
                return false;
        }else{
            //(cx-1,cy-1)
            if(selectPointInChunk(cx-1,cy,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
            if(selectPointInChunk(cx-1,cy-1,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
            if(selectPointInChunk(cx  ,cy-1,ray,outCollisionPoint,outTriangle,outNode))
                return true;
            else
                return false;
        }
    }
}


terrain::chunk * terrain::getChunkFromStr(const char * str){
    int x,y;
    if(sscanf(str,"tc %d %d",&x,&y)==2){
        chunkmtx.lock();
        auto it=chunks.find(ipair(x,y));
        if(it!=chunks.end()){
            chunkmtx.unlock();
            return it->second;
        }
        chunkmtx.unlock();
    }
    return NULL;
}

terrain::item *  terrain::getItemFromStr(const char * str){
    int x,y,itemid;
    long id;
    if(sscanf(str,"ti %d %d %ld %d",&x,&y,&id,&itemid)==4){
        auto it=allItems.find(mapid(x,y,id,itemid));
        if(it!=allItems.end()){
            return it->second;
        }
    }
    return NULL;
}

}//namespace smoothly

