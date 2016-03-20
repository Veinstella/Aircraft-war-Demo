#include "GameScene.h"
#include "SimpleAudioEngine.h"
#include "PlaneHero.h"
#include "PlaneEnemy.h"
#include "GameOverScene.h"

using namespace CocosDenshion;

GameScene* GameScene::m_gamelayer = nullptr;

const float GameScene::refresh_delay[] = {2.0, 1.5, 1.0, 0.5, 0.2}; //战机刷新间隔

Scene* GameScene::createScene()
{
	//创建一个没有重力的物理世界
	auto scene = Scene::createWithPhysics();
	scene->getPhysicsWorld()->setGravity(Vect(0, 0));

	//物理调试绘图
	//scene->getPhysicsWorld()->setDebugDrawMask(PhysicsWorld::DEBUGDRAW_ALL);

	m_gamelayer = GameScene::create();////游戏场景层的单例对象
	scene->addChild(m_gamelayer);

	return scene;
}

bool GameScene::init()
{
	Layer::init(); 
	log("Game init!");

	auto winSize = Director::getInstance()->getWinSize();

	//加载plist文件
	//???
	SpriteFrameCache::getInstance()->addSpriteFramesWithFile("shoot.plist");
	SpriteFrameCache::getInstance()->addSpriteFramesWithFile("shoot_background.plist");

	//加载音乐，播放背景音乐
	auto audioengine = SimpleAudioEngine::getInstance();
	audioengine->preloadBackgroundMusic("sound/bgmusic.mp3");
	audioengine->preloadEffect("sound/shoot.wav");
	audioengine->preloadEffect("sound/hero_down.mp3");
	audioengine->preloadEffect("sound/enemy3_down.wav");
	audioengine->preloadEffect("sound/enemy1_down.wav");
	audioengine->preloadEffect("sound/enemy2_down.wav");
	audioengine->preloadEffect("sound/big_spaceship_flying.mp3");

	audioengine->playBackgroundMusic("sound/bgmusic.mp3", true);

	//播放背景图
	playBackground();

	//加入暂停按钮
	auto pause_sp1 = Sprite::createWithSpriteFrameName("game_pause_nor.png");
	auto pause_sp2 = Sprite::createWithSpriteFrameName("game_pause_pressed.png");
	auto resume_sp1 = Sprite::createWithSpriteFrameName("game_resume_nor.png");
	auto resume_sp2 = Sprite::createWithSpriteFrameName("game_resume_pressed.png");

	auto pauseitem = MenuItemSprite::create(pause_sp1, pause_sp2);
	auto resumeitem = MenuItemSprite::create(resume_sp1, resume_sp2);
	auto pauseButton = MenuItemToggle::createWithTarget(this, menu_selector(GameScene::pauseButtonCallBack), pauseitem, resumeitem, nullptr);
	pauseButton->setAnchorPoint(Vec2(0, 0));
	pauseButton->setPosition(Vec2(winSize.width /2 - pauseButton->getContentSize().width, winSize.height /2 - pauseButton->getContentSize().height)); //位于右上角
	
	auto menu = Menu::create(pauseButton, nullptr);
	addChild(menu, 10, PAUSE_MENU);

	//加入一个分数栏
	m_score = 0;
	auto scorelabel = Label::createWithSystemFont("Score: 0", "Arial", 36);
	scorelabel->setAnchorPoint(Vec2(0, 0));
	scorelabel->setPosition(Vec2(0, winSize.height - scorelabel->getContentSize().height)); //位于左上角
	addChild(scorelabel, 10, SCORE_LABEL);

	//加入我方战机，初始位置设定在下方中点
	auto hero = PlaneHero::create();
	addChild(hero, 0, HERO_TAG);
	hero->setPosition(Vec2(winSize.width / 2, hero->getContentSize().height / 2 + 10));
	//auto herobody = PhysicsBody::createBox(hero->getContentSize()); //这样设置不太精准
	auto herobody = PhysicsBody::create();
	Vec2 verts[] = {Vec2(0, 55), Vec2(50, -30), Vec2(-50, -30)};  //根据点组成一个多边形
	herobody->addShape(PhysicsShapeEdgePolygon::create(verts, 3));
	herobody->setCollisionBitmask(0x0); //不进行碰撞模拟
	herobody->setContactTestBitmask(HERO_CONTACTMASKBIT);
	hero->setPhysicsBody(herobody);

	//加入一个触摸监听，用来移动我方战机
	auto touchlistener = EventListenerTouchOneByOne::create();
	touchlistener->setSwallowTouches(true);
	touchlistener->onTouchBegan = [this](Touch *pTouch, Event*) {
		//触摸着飞机移动
		auto hero = (PlaneHero*)getChildByTag(HERO_TAG);
		if (hero->boundingBox().containsPoint(pTouch->getLocation()))
		{
			//log("choose hero!");
			return true;
		}
		else return false;
	};
	touchlistener->onTouchMoved = [this](Touch* pTouch, Event*) {
		auto delta = pTouch->getDelta();
		auto hero = (PlaneHero*)getChildByTag(HERO_TAG);
		auto oldpos = hero->getPosition(); //用来重置出界后的位置

		hero->setPosition(Vec2(hero->getPosition().x + delta.x, hero->getPosition().y + delta.y));
		
		//不能出界
		auto winSize = Director::getInstance()->getWinSize();
		if (hero->getPositionY() < hero->getContentSize().height / 2 || hero->getPositionY() > winSize.height - hero->getContentSize().height / 2
			|| hero->getPositionX() < hero->getContentSize().width / 2 || hero->getPositionX() > winSize.width - hero->getContentSize().width / 2)
		{
			hero->setPosition(oldpos);
		}
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(touchlistener, this);	

	//加入子弹渲染集合
	m_bulletBox = SpriteBatchNode::createWithTexture(hero->getTexture()); //这里获得的是一张纹理大图（整张png）
	addChild(m_bulletBox);

	//加入碰撞事件监听
	auto contactlistener = EventListenerPhysicsContact::create();
	contactlistener->onContactBegin = CC_CALLBACK_1(GameScene::dealWithContact, this);
	_eventDispatcher->addEventListenerWithSceneGraphPriority(contactlistener, this);	

	//开启Level检测，根据检测出的Level调整战机的刷新速度
	m_level = LEVEL1;
	schedule(schedule_selector(GameScene::testLevel), 1.0f);

	//设置初始敌机刷新间隔并开始刷新
	schedule(schedule_selector(GameScene::refreshAnEnemy), refresh_delay[LEVEL1]);
	
	//每隔5秒可以刷新一次boss战机
	m_canBossRefresh = true;
	schedule(schedule_selector(GameScene::resetBoss), 5.0f); 

	return true;
}

void GameScene::playBackground()
{
	auto winSize = Director::getInstance()->getWinSize();

	int delta = 10; //补缝用的，两个背景紧挨着移动可能出现缝隙

	//播放背景，两个背景的图片是一样的，紧挨着循环播放
	//注意点：1.轮播图的像素高应该比设备的大一点 2.后面的图应该稍微往前靠一点 这样就不会有缝隙了
	auto bg1 = Sprite::createWithSpriteFrameName("background.png");
	bg1->setScaleX(winSize.width / bg1->getContentSize().width);
	bg1->setScaleY(winSize.height / bg1->getContentSize().height + 0.01);
	bg1->setAnchorPoint(Vec2(0, 0));
	addChild(bg1);

	auto move1 = MoveBy::create(30, Vec2(0, -winSize.height));
	auto callfunc1 = CallFunc::create([bg1](){
		bg1->setPosition(Vec2(0, 0));
	});
	auto action1 = Sequence::create(move1, callfunc1, nullptr);
	bg1->runAction(RepeatForever::create(action1));

	auto bg2 = Sprite::createWithSpriteFrameName("background.png");
	bg2->setScaleX(winSize.width / bg2->getContentSize().width);
	bg2->setScaleY(winSize.height / bg2->getContentSize().height + 0.01);
	bg2->setAnchorPoint(Vec2(0, 0));
	bg2->setPosition(Vec2(0, bg1->getPositionY() + winSize.height - delta));
	addChild(bg2);
	
	auto move2 = MoveTo::create(30, Vec2(0, 0));
	auto callfunc2 = CallFunc::create([bg2, bg1, winSize, delta](){
		bg2->setPosition(Vec2(0, bg1->getPositionY() + winSize.height - delta));
	});
	auto action2 = Sequence::create(move2, callfunc2, nullptr);
	bg2->runAction(RepeatForever::create(action2));
}

void GameScene::publishScore()
{
	auto userdefault = UserDefault::getInstance();
	
	//查看路径，测试用
	//log(userdefault->getXMLFilePath().c_str()); 
	
	//存储本次游戏分数
	char score_str[100] = {0};
	sprintf(score_str, "%d", m_score);
	userdefault->setStringForKey("LastScore", score_str);

	//存储最佳游戏分数
	auto bestscore = userdefault->getStringForKey("BestScore");
	if (m_score > atoi(bestscore.c_str()))
		userdefault->setStringForKey("BestScore", score_str);
}

void GameScene::gameover()
{
	publishScore(); //存储游戏分数
	auto scene = TransitionFade::create(2.0, GameOverScene::createScene());
	Director::getInstance()->replaceScene(scene);
}

void GameScene::pauseButtonCallBack(Ref* pSender)
{
	//log("press pause button!");

	/*
	 *  暂停/开启 步骤：
	 *1.关闭/开启 触摸监听
	 *2.遍历子节点，把除了暂停按钮的 关闭/开启 渲染(onExit)
	 *3.关闭/开启 敌机的刷新schedule
	 *4.关闭/开启 物理引擎的模拟
	*/
	static int i = 0;
	if (0 == i)
	{
		i = 1;

		_eventDispatcher->pauseEventListenersForTarget(this);
		
		auto vec = this->getChildren();
		for (auto &child : vec)
		{
			if (child->getTag() != PAUSE_MENU)
				child->onExit();
		}

		unschedule(schedule_selector(GameScene::refreshAnEnemy));
		SimpleAudioEngine::getInstance()->pauseBackgroundMusic();
		((Scene*)this->getParent())->getPhysicsWorld()->setAutoStep(false);
		
	}
	else
	{
		i = 0;

		_eventDispatcher->resumeEventListenersForTarget(this);
		
		auto vec = this->getChildren();
		for (auto &child : vec)
		{
			if (child->getTag() != PAUSE_MENU)
				child->onEnter();
		}

		schedule(schedule_selector(GameScene::refreshAnEnemy), refresh_delay[m_level]);
		SimpleAudioEngine::getInstance()->resumeBackgroundMusic();
		((Scene*)this->getParent())->getPhysicsWorld()->setAutoStep(true);
	}
}

bool GameScene::dealWithContact(PhysicsContact& contact)
{
	//log("contact begin!");
	//获取两个碰撞的节点
	auto node1 = contact.getShapeA()->getBody()->getNode();
	auto node2 = contact.getShapeB()->getBody()->getNode();

	if (!node1 || !node2)
		return false;

	auto tag1 = node1->getTag();
	auto tag2 = node2->getTag();

	//如果碰撞的是子弹和敌方飞机
	if ((tag1 == HERO_BULLET_TAG && tag2 ==ENEMY_TAG) || (tag2 == HERO_BULLET_TAG && tag1 ==ENEMY_TAG))
	{
		PlaneEnemy* enemy = nullptr;
		if (tag1 == HERO_BULLET_TAG)
		{
			node1->removeFromParent();
			enemy =((PlaneEnemy*)node2);
		}
		else
		{
			node2->removeFromParent();
			enemy =((PlaneEnemy*)node1);
		}

		hitEnemy(enemy);
	}

	//如果碰撞的是我方战机和敌机
	if ((tag1 == HERO_TAG && tag2 == ENEMY_TAG) || (tag2 == HERO_TAG && tag1 == ENEMY_TAG))
	{
		_eventDispatcher->removeEventListenersForType(EventListener::Type::TOUCH_ONE_BY_ONE); //不再接受触摸事件

		PlaneHero *hero = nullptr;
		PlaneEnemy *enemy = nullptr;
		if (tag1 == HERO_TAG)
		{
			hero = ((PlaneHero*)node1);
			enemy = ((PlaneEnemy*)node2);
		}
		else
		{
			hero = ((PlaneHero*)node2);
			enemy = ((PlaneEnemy*)node1);
		}

		hitEnemy(enemy);
		hitHero(hero);
	}

	return true;
}

void GameScene::hitEnemy(PlaneEnemy* enemy)
{
	enemy->getHurt();
		
	//如果敌方飞机挂了，增加分数
	if (!enemy->isLive())
	{
		m_score += enemy->getPoints();
		char buf[100] = {0};
		sprintf(buf, "Score: %d", m_score);
		auto scorelabel = (Label*)this->getChildByTag(SCORE_LABEL);
		scorelabel->setString(buf);
	}
}

void GameScene::hitHero(PlaneHero* hero)
{
	//我机直接死亡
	hero->dead();
}

void GameScene::testLevel(float dt)
{
	int oldlevel = m_level;

	//根据到达的分数，设置LEVEL
	if (m_score > Level1Up_Score && m_score < Level2Up_Score)
		m_level = LEVEL2;
	else if (m_score > Level2Up_Score && m_score < Level3Up_Score)
		m_level = LEVEL3;
	else if (m_score > Level3Up_Score && m_score < Level4Up_Score)
		m_level = LEVEL4;
	else if (m_score > Level4Up_Score)
		m_level = LEVEL5;

	//如果level没变化，不需要更改刷新速度
	if (oldlevel == m_level)
		return;

	schedule(schedule_selector(GameScene::refreshAnEnemy), refresh_delay[m_level]); //更新战机刷新速度
}

void GameScene::refreshAnEnemy(float dt)
{
	//log("refresh an enemy");

	auto winSize = Director::getInstance()->getWinSize();

	int enemy_type = PlaneEnemy::Enemy1;

	//根据游戏Level选择性随机刷新不同种类的战机
	switch (m_level)
	{
	case LEVEL1:
		break;
	case LEVEL2:
		enemy_type = random(0, 1);
		break;
	case LEVEL3:
		enemy_type = random(0, 2);
		if (m_canBossRefresh && enemy_type == 2) //如果刷新出boss战机，那么禁止其继续刷新，直到该禁止标志被重置
			m_canBossRefresh = false;
		else enemy_type = random(0, 1);
		break;
	case LEVEL4:
		enemy_type = random(0, 3);
		if (m_canBossRefresh && enemy_type >= 2)
			m_canBossRefresh = false;
		else enemy_type = random(0, 1);
		break;
	case LEVEL5:
		enemy_type = random(0, 3);
		if (m_canBossRefresh && enemy_type >= 2)
			m_canBossRefresh = false;
		else enemy_type = random(0, 1);
		break;
	}

	//根据战机类型加入战机
	auto enemy = PlaneEnemy::createWithEnemyType(enemy_type);
	addChild(enemy, 0, ENEMY_TAG);

	//设定战机初始位置的X轴的取值范围，根据这个范围随机设置战机初始X轴位置
	int min = enemy->getContentSize().width / 2;
	int max = winSize.width - enemy->getContentSize().width / 2;
	enemy->setPosition(Vec2(random(min, max), winSize.height + enemy->getContentSize().height / 2));

	//给敌机一个body
	Vec2 vec[10]; //存放敌方战机的多边形点
	memset(vec, 0, sizeof(vec));
	int vec_count = 0;
	switch (enemy_type)
	{
	case PlaneEnemy::Enemy1:
		vec[0] = Vec2(0, -17);
		vec[1] = Vec2(-22, 5);
		vec[2] = Vec2(22, 5);
		vec_count = 3;
		break;
	case PlaneEnemy::Enemy2:
		vec[0] = Vec2(0, -40);
		vec[1] = Vec2(-40, 0);
		vec[2] = Vec2(0, 45);
		vec[3] = Vec2(40, 0);
		vec_count = 4;
		break;
	default:
		vec[0] = Vec2(50, -100);
		vec[1] = Vec2(-50, -100);
		vec[2] = Vec2(-54, -85);
		vec[3] = Vec2(-54, 90);
		vec[4] = Vec2(54, 90);
		vec[5] = Vec2(67, -88);
		vec_count = 6;
		break;
	}

	//auto enemybody = PhysicsBody::createBox(enemy->getContentSize());
	auto enemybody = PhysicsBody::create();
	enemybody->addShape(PhysicsShapePolygon::create(vec, vec_count));
	enemybody->setCollisionBitmask(0x0); //不进行碰撞模拟，因为不需要
	enemybody->setContactTestBitmask(ContactMaskBit::ENEMY_CONTACTMASKBIT);
	enemy->setPhysicsBody(enemybody);
}