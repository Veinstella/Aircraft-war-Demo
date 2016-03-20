#include "PlaneHero.h"'
#include "GameScene.h"
#include "SimpleAudioEngine.h"

using namespace CocosDenshion;

bool PlaneHero::init()
{
	Sprite::init();
	
	//初始化纹理图
	initWithSpriteFrameName("hero2.png");

	return true;
}

void PlaneHero::onEnter()
{
	Sprite::onEnter();

	//进入后，开始发射子弹
	schedule(schedule_selector(PlaneHero::beginShooting), .5f);
	//检查子弹是否出界，出界则清除出bulletBox
	schedule(schedule_selector(PlaneHero::removeBullet), 0.1);
}

void PlaneHero::dead()
{
	getPhysicsBody()->setContactTestBitmask(0x0); ////设置碰撞标志位，不再发生碰撞事件
	blowUp();
}

void PlaneHero::blowUp()
{
	this->stopShooting();
	SimpleAudioEngine::getInstance()->playEffect("sound/hero_down.mp3");//播放音效

	//加载死亡动画
	auto animation = Animation::create();
	animation->addSpriteFrame(SpriteFrameCache::getInstance()->getSpriteFrameByName("hero_blowup_n1.png"));//添加一个SpriteFrame到动画中
	animation->addSpriteFrame(SpriteFrameCache::getInstance()->getSpriteFrameByName("hero_blowup_n2.png"));
	animation->addSpriteFrame(SpriteFrameCache::getInstance()->getSpriteFrameByName("hero_blowup_n3.png"));
	animation->addSpriteFrame(SpriteFrameCache::getInstance()->getSpriteFrameByName("hero_blowup_n4.png"));
	animation->setDelayPerUnit(0.2);//总时间，以秒为单位。
	animation->setRestoreOriginalFrame(true);//设置是否在动画结束时恢复至初始帧。

	auto blowUp = Animate::create(animation);//使用Animation（动画）对象创建一个Animate对象，目标精灵的初始帧将在动画结束后被恢复。
	
	//爆炸完后结束游戏
	auto gameoverCall = CallFunc::create([this]() {
		//log("call gameover!");
		removeFromParent();
		GameScene::sharedGameLayer()->gameover();
	});

	this->runAction(Sequence::create(blowUp, gameoverCall, nullptr));/* runAction执行一个动作，并且返回执行的该动作。 Sequence::create(); 使用指定动作数组来创建可以按序列执行的Sequence动作对象.
																	  * create()使用了变长数组*/
}

void PlaneHero::beginShooting(float dt)
{
	SimpleAudioEngine::getInstance()->playEffect("sound/shoot.wav");

	//log("a bullet shoot!");
	//创建一颗子弹，加入到GameLayer的子弹渲染器中
	auto bullet = Sprite::createWithSpriteFrameName("bullet1.png");
	bullet->setPosition(Vec2(getPositionX(), getPositionY() + getContentSize().height / 2));
	auto bulletBox = GameScene::sharedGameLayer()->getBulletBox();//?
	
	bulletBox->addChild(bullet, 0, GameScene::HERO_BULLET_TAG);

	//给子弹套一个body，加一个初始速度，让其射到物理世界中
	auto body = PhysicsBody::createBox(bullet->getContentSize(), PhysicsMaterial(0, 0, 0));
	body->setVelocity(Vect(0, 500)); 
	body->setContactTestBitmask(GameScene::ContactMaskBit::HERO_BULLET_CONTACTMASKBIT);
	body->setCollisionBitmask(0x0); //不进行碰撞模拟，因为不需要。

	bullet->setPhysicsBody(body);
}

void PlaneHero::removeBullet(float dt)
{
	//遍历每一颗子弹，把出界的删除
	auto bulletBox = GameScene::sharedGameLayer()->getBulletBox();
	auto vec = bulletBox->getChildren();
	for (auto &bullet : vec)
	{
		auto posInNode = bullet->getPosition();
		//由于bullet处于bulletBox的模型坐标系，因此要转化成世界坐标系，不过如果子弹渲染器在原点则不是必要
		auto posInWorld = bulletBox->convertToWorldSpace(posInNode);
		if (posInWorld.y > Director::getInstance()->getVisibleSize().height)
		{
			bulletBox->removeChild(bullet, true);
			//log("a bullet remove!");
		}
	}
}

void PlaneHero::stopShooting()
{
	unschedule(schedule_selector(PlaneHero::beginShooting));
}
