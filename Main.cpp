# include <Siv3D.hpp> // Siv3D v0.6.15
# include "Multiplayer_Photon.hpp"
# include "PHOTON_APP_ID.SECRET"


InputGroup KeyGroupLeft{ KeyLeft, KeyA };
InputGroup KeyGroupRight{ KeyRight, KeyD };
InputGroup KeyGroupUp{ KeyUp, KeyW };
InputGroup KeyGroupDown{ KeyDown, KeyS };

enum class NetWorkState {
	Disconnected,
	Connecting,
	InLobby,
	Joining,
	InRoom,
	Leaving,
	Disconnecting,
};

[[nodiscard]]
StringView ToString(NetWorkState state) {
	switch (state) {
	case NetWorkState::Disconnected:
		return U"Disconnected";
	case NetWorkState::Connecting:
		return U"Connecting";
	case NetWorkState::InLobby:
		return U"InLobby";
	case NetWorkState::Joining:
		return U"Joining";
	case NetWorkState::InRoom:
		return U"InRoom";
	case NetWorkState::Leaving:
		return U"Leaving";
	case NetWorkState::Disconnecting:
		return U"Disconnecting";
	default:
		return U"Unknown";
	}
}

void drawCrown(const Vec2& pos) {
	/*constexpr double w = 20;
	constexpr double h = 15;
	constexpr Vec2 bottom_left = Vec2{ -w/2,0 };
	constexpr Vec2 left = Vec2{ -w/2,-h };
	constexpr Vec2 leftV = Vec2{ -w / 3,-h / 2 };
	constexpr Vec2 top = Vec2{ 0,-h };
	constexpr Vec2 rightV = Vec2{ w / 3,-h / 2 };
	constexpr Vec2 right = Vec2{ w/2,-h };
	constexpr Vec2 bottom_right =  Vec2{ w/2,0 };
	Transformer2D tf{ Mat3x2::Translate(pos) };
	Polygon{ bottom_left,left,leftV,top,rightV,right,bottom_right }.draw(Palette::Yellow);*/
	const ScopedRenderStates2D sampler{ SamplerState::ClampNearest };
	TextureAsset(U"gold_crown").scaled(1).drawAt(pos);
}

struct Player {
	Vec2 pos;
	bool isTransparent = false;
	bool isWatching = false;
	bool isSlowdown = false;
	ColorF color;
	String name;

	Player() = default;
	Player(const Vec2& pos,const Color& color,const String& name) : pos(pos),color(color),name(name),isTransparent(false),isWatching(false) {}

	template <class Archive>
	void SIV3D_SERIALIZE(Archive& archive)
	{
		archive(pos, isTransparent, isWatching, isSlowdown, color, name);
	}
};

struct Trap {
	Vec2 pos;
	LocalPlayerID ownerID;
	Color color = Palette::White;
	template <class Archive>
	void SIV3D_SERIALIZE(Archive& archive)
	{
		archive(pos, ownerID, color);
	}
};

class ShareRoomData {

public:
	ShareRoomData() = default;

	const HashTable<LocalPlayerID, Player>& players() const {
		return m_players;
	}

	const LocalPlayerID itID() const {
		return m_itID;
	}

	const HashTable<size_t,Trap>& traps() const {
		return m_traps;
	}

	void setPlayerPos(LocalPlayerID id, Vec2 pos) {
		m_players.at(id).pos = pos;
	}

	void addPlayer(LocalPlayerID id, Vec2 pos, Color color, String name) {
		m_players.insert_or_assign(id, Player(pos, color, name));
	}

	void erasePlayer(LocalPlayerID id) {
		m_players.erase(id);
	}

	void beTransparent(LocalPlayerID id, bool beTransparent) {
		m_players.at(id).isTransparent = beTransparent;
	}

	void beWatching(LocalPlayerID id, bool beWatching) {
		m_players.at(id).isWatching = beWatching;
	}

	void beSlowdown(LocalPlayerID id, bool beSlowdown) {
		m_players.at(id).isSlowdown = beSlowdown;
	}

	void setPlayerName(LocalPlayerID id, const String& name) {
		m_players.at(id).name = name;
	}

	void setItID(LocalPlayerID id) {
		m_itID = id;
	}

	void addTrap(const Vec2& pos, LocalPlayerID ownerID,const Color& color) {
		m_traps.insert_or_assign(nextTrapID,Trap{ pos,ownerID,color});
		nextTrapID++;
	}

	void eraseTrap(size_t id) {
		m_traps.erase(id);
	}

	void eraseTrap(LocalPlayerID ownerID) {
		for (auto it = m_traps.begin(); it != m_traps.end();) {
			if (it->second.ownerID == ownerID) {
				it = m_traps.erase(it);
			}
			else {
				++it;
			}
		}
	}

	void clearTraps() {
		m_traps.clear();
	}

	template <class Archive>
	void SIV3D_SERIALIZE(Archive& archive)
	{
		archive(m_players, m_itID, m_traps, nextTrapID);
	}
private:
	HashTable<LocalPlayerID,Player> m_players;
	LocalPlayerID m_itID = 0;
	HashTable<size_t,Trap> m_traps;
	size_t nextTrapID = 0;
};

//enum class EventCode:uint8 {
//	roomDataFromHost,
//	playerAdd,
//	playerErase,
//	playerMove,
//	beTransparent,
//	beWatching,
//	beSlowdown,
//	playerNameChange,
//	itIDChange,
//	tagStop,
//	addTrap,
//	requestTrappedToHost,
//	solveTrapped,
//	eraseTrap,
//};

namespace EventCode {
	enum : uint8 {
		roomDataFromHost,
		playerAdd,
		playerErase,
		playerMove,
		beTransparent,
		beWatching,
		beSlowdown,
		playerNameChange,
		itIDChange,
		tagStop,
		addTrap,
		requestTrappedToHost,
		solveTrapped,
		eraseTrap,
	};
}
uint8 a = EventCode::roomDataFromHost;
class MyNetwork : public Multiplayer_Photon
{
public:
	using Multiplayer_Photon::Multiplayer_Photon;

	NetWorkState state = NetWorkState::Disconnected;
	bool isFirstConnecting = true;

	//Lobby Data
	TextEditState roomNameBox;
	TextEditState userNameBox;
	void initWhenEnterLobby() {
		roomNameBox = TextEditState{ U"room" };
		userNameBox = TextEditState{ U"player" };
	}
	void updateLobby() {

		SimpleGUI::TextBox(roomNameBox, Vec2{ 100,20 }, 200);
		if (SimpleGUI::Button(U"ルームを作成", Vec2{ 100,70 })) {
			initWhenCreateRoom();
			createRoom(roomNameBox.text + U"#" + ToHex(RandomUint16()), 20);
			state = NetWorkState::Joining;
		}

		SimpleGUI::TextBox(userNameBox, Vec2{ 580,20 }, 200);

		for(auto [i,roomName] : Indexed(getRoomNameList())){
			if (SimpleGUI::Button(roomName, Vec2{ 100 + (i % 3) * 200,150 + (i / 3) * 50 })) {
				initWhenJoinRoom();
				joinRoom(roomName);
				state = NetWorkState::Joining;
			}
		}
	}
	

	//Room Data
	static constexpr double playerRadius = 20;
	static constexpr double trapBodyRadius = 7;

	bool hasRoomData = false;
	ShareRoomData roomData;
	P2World world;
	static constexpr double stepTime = 1.0 / 200.0;
	double accumulatedTime = 0.0;
	Array<P2Body> walls;
	P2Body playerBody;
	Timer tagStoppingTimer;
	Stopwatch noMovingTime;
	double trapAccumulatedTime = 0.0;
	constexpr static double trapStepTime = 5;
	constexpr static Duration slowDownTime = 2.0s;

	Vec2 spawnPos = Vec2{ 400,300 };

	struct PlayerLocalData {
		PlayerLocalData() = default;
		PlayerLocalData(const Vec2& pos) : pos(pos) {
			animationOffset = Random(0.0, 10.0);
		}
		Vec2 pos;
		Vec2 velocity{};
		Timer fadeoutTimer;
		bool isFacingRight = true;
		double animationOffset = 0.0;
		Timer slowdownTimer;
	};

	HashTable<LocalPlayerID, PlayerLocalData> playersLocalData;

	void flipFadeoutTimer(const LocalPlayerID playerID) {
		constexpr Duration fadeoutTime = 0.1s;
		Timer& fadeoutTimer = playersLocalData.at(playerID).fadeoutTimer;
		Duration remain = fadeoutTimer.remaining();
		fadeoutTimer.restart(fadeoutTime);
		fadeoutTimer.setRemaining(fadeoutTime - remain);
	}

	const Player& getPlayer() const {
		return roomData.players().at(getLocalPlayerID());
	}

	bool isIt() const {
		return getLocalPlayerID() == roomData.itID();
	}
	
	void createWalls() {
		world = P2World{ {0, 0} };

		walls << world.createRect(P2Static, Scene::Rect().topCenter(), Size{ Scene::Width(),100 });
		walls << world.createRect(P2Static, Scene::Rect().bottomCenter(), Size{ Scene::Width(),100 });
		walls << world.createRect(P2Static, Scene::Rect().leftCenter(), Size{ 100,Scene::Height() });
		walls << world.createRect(P2Static, Scene::Rect().rightCenter(), Size{ 100,Scene::Height() });

		walls << world.createRect(P2Static, Vec2{ 200,200 }, Size{ 50,50 });
		walls << world.createRect(P2Static, Vec2{ 600,300 }, Size{ 50,50 }).setAngle(45_deg);
		walls << world.createRect(P2Static, Vec2{ 250,450 }, Size{ 150,30 });
		walls << world.createRect(P2Static, Vec2{ 440,250 }, Size{ 30,200 }).setAngle(0_deg);
	}

	void initRoomData() {
		hasRoomData = false;
		roomData = ShareRoomData{};
		createWalls();
		playerBody = world.createCircle(P2Dynamic, { 400,300 }, 15).setFixedRotation(true);
		accumulatedTime = 0.0;
		trapAccumulatedTime = 0.0;
		playersLocalData.clear();
	}

	void initWhenCreateRoom() {
		initRoomData();
		hasRoomData = true;
	}

	void initWhenJoinRoom() {
		initRoomData();
		hasRoomData = false;
	}

	LocalPlayerID hostID() const {
		for (auto& player : getLocalPlayers()) {
			if (player.isHost) {
				return player.localID;
			}
		}
		return 0;
	}

	void updateRoom(double delta = Scene::DeltaTime()) {
		
		if(not hasRoomData)return;
		Vec2 inputAxis = Vec2(KeyGroupRight.pressed() - KeyGroupLeft.pressed(), KeyGroupDown.pressed() - KeyGroupUp.pressed());
		//Vec2 inputAxis = Vec2(KeyD.pressed() - KeyA.pressed(), KeyS.pressed() - KeyW.pressed());
		bool beTransparent = KeySpace.pressed();
		Vec2 prePos = playerBody.getPos();
		Vec2 normalizedInputAxis = inputAxis.setLength(1);

		double speed = beTransparent ? 120 : 200;
		if (isIt()) {
			speed *= 1.1;
		}

		if (getPlayer().isSlowdown) {
			speed *= 0.5;
		}

		if(tagStoppingTimer.isRunning() and isIt()){
			speed = 0;
		}

		playerBody.setVelocity(normalizedInputAxis * speed);

		for (accumulatedTime += delta; accumulatedTime >= stepTime; accumulatedTime -= stepTime) {
			world.update(stepTime);
		}

		Vec2 pos = playerBody.getPos();
		if (prePos != pos) {
			roomData.setPlayerPos(getLocalPlayerID(), pos);
			sendEvent(FromEnum(EventCode::playerMove), Serializer<MemoryWriter>{}(pos));
			noMovingTime.restart();
			roomData.beWatching(getLocalPlayerID(), false);
			sendEvent(FromEnum(EventCode::beWatching), Serializer<MemoryWriter>{}(false));
		}
		if(beTransparent){
			noMovingTime.restart();
			roomData.beWatching(getLocalPlayerID(), false);
			sendEvent(FromEnum(EventCode::beWatching), Serializer<MemoryWriter>{}(false));
		}

		if (beTransparent != getPlayer().isTransparent) {
			roomData.beTransparent(getLocalPlayerID(), beTransparent);
			flipFadeoutTimer(getLocalPlayerID());
			sendEvent(FromEnum(EventCode::beTransparent), Serializer<MemoryWriter>{}(beTransparent));
		}

		if(noMovingTime > 1.0s){
			if (not getPlayer().isWatching) {
				roomData.beWatching(getLocalPlayerID(), true);
				sendEvent(FromEnum(EventCode::beWatching), Serializer<MemoryWriter>{}(true));
			}
		}

		for (auto& [id, player] : roomData.players()) {
			Vec2& velocity = playersLocalData.at(id).velocity;
			playersLocalData.at(id).pos = Math::SmoothDamp(playersLocalData.at(id).pos, player.pos, velocity, 1.0 / 20, unspecified, delta);
			bool& isFacingRight = playersLocalData.at(id).isFacingRight;
			if (velocity.x > 10) {
				isFacingRight = true;
			}
			else if (velocity.x < -10) {
				isFacingRight = false;
			}
			
		}


		if (not tagStoppingTimer.isRunning() and isIt()) {
			for (auto& [id, player] : roomData.players()) {
				if (id == getLocalPlayerID())continue;

				if (playerBody.getPos().asCircle(playerRadius).intersects(playersLocalData.at(id).pos.asCircle(playerRadius))) {
					roomData.setItID(id);
					sendEvent(FromEnum(EventCode::itIDChange), Serializer<MemoryWriter>{}(id));

					tagStoppingTimer.restart(3.0s);
					roomData.clearTraps();
					trapAccumulatedTime = 0.0;
					sendEvent(FromEnum(EventCode::tagStop), Serializer<MemoryWriter>{});
				}
			}
		}

		for (trapAccumulatedTime += delta; trapAccumulatedTime >= trapStepTime; trapAccumulatedTime -= trapStepTime) {

			if (getPlayer().isTransparent) continue;
			HSV hsv = HSV(getPlayer().color);
			hsv.s = 0.5;
			Color color = hsv;
			roomData.addTrap(playerBody.getPos(), getLocalPlayerID(), color);
			sendEvent(FromEnum(EventCode::addTrap), Serializer<MemoryWriter>{}(playerBody.getPos(),color));
		}


		for (auto& [trapID,trap] : roomData.traps()) {
			if (trap.ownerID == getLocalPlayerID())continue;
			if (playerBody.getPos().asCircle(playerRadius).intersects(trap.pos.asCircle(trapBodyRadius))) {

				sendEvent({FromEnum(EventCode::requestTrappedToHost),Array{ hostID() }},trapID);
			}
		}

		if (getPlayer().isSlowdown and not playersLocalData.at(getLocalPlayerID()).slowdownTimer.isRunning()) {
			roomData.beSlowdown(getLocalPlayerID(), false);
			sendEvent(FromEnum(EventCode::beSlowdown), Serializer<MemoryWriter>{}(false));
		}

	}

	void drawRoom() {

		if (not hasRoomData) {
			FontAsset(U"message")(U"データを受信中...").drawAt(Scene::Center(), Palette::White);
			return;
		}

		{
			ScopedRenderStates2D sampler{ SamplerState::ClampNearest };
			TextureAsset(U"boseki").scaled(2).drawAt(spawnPos);
		}
		{
			ScopedRenderStates2D sampler{ SamplerState::ClampNearest };
			for (auto& [id,trap] : roomData.traps()) {
				
				int32 page = static_cast<int32>(Scene::Time() / 0.25) % 4;

				TextureAsset(U"pow")(page % 2 * 32, page / 2 * 32, 32, 32).scaled(2).drawAt(trap.pos, trap.color);
			}
		}
		

		for (auto& wall : walls) {
			wall.draw(Color{ 79, 79, 79 });
			
		}

		auto drawGoast = [&](const Vec2& pos, double alpha,LocalPlayerID id,const Player& player,const PlayerLocalData& localPlayer) {
			ScopedColorMul2D scm{ 1.0,1.0,1.0,alpha };
			{
				ScopedRenderStates2D sampler{ SamplerState::ClampNearest };
				int32 i = static_cast<int32>((Scene::Time() + localPlayer.animationOffset) / 1.2) % 2;

				Color color = player.color;
				if(player.isSlowdown){
					double t = Periodic::Sine0_1(0.5s, localPlayer.slowdownTimer.sF());
					HSV hsv = HSV(color);
					hsv.v = Math::Map(t, 0, 1, 1, 0.5);
					color = hsv;
				}

				TextureAsset(U"goast_body")(0, 32 * i, 32, 32).scaled(2).mirrored(localPlayer.isFacingRight).drawAt(pos, color);

				if (player.isWatching) {

					TextureAsset(U"goast_eye")(0, 32 * 2, 32, 32).scaled(2).mirrored(localPlayer.isFacingRight).drawAt(pos);
				}
				else {
					TextureAsset(U"goast_eye")(0, 0, 32, 32).scaled(2).mirrored(localPlayer.isFacingRight).drawAt(pos);
				}
			}
			double x_sign = localPlayer.isFacingRight ? 1 : -1;
			if (id == roomData.itID())drawCrown(pos + Vec2(x_sign * 3, -30 + Periodic::Sine1_1(2) * 3));

			//Circle{ pos,playerRadius }.drawFrame(2, Palette::Black);

			FontAsset(U"name")(player.name).drawAt(pos + Vec2{ 0,30 }, ColorF(1));
		};

		for (auto& [id, player] : roomData.players()) {
			if (id == getLocalPlayerID())continue;
			const PlayerLocalData& localPlayer = playersLocalData.at(id);

			double alpha = 1.0;
			if (player.isTransparent) {
				alpha = localPlayer.fadeoutTimer.progress1_0();
				if(noMovingTime > 1.0s){
					alpha = Min((noMovingTime.sF() - 1.0), 0.5);
				}
			}
			else {
				alpha = localPlayer.fadeoutTimer.progress0_1();
			}
			if(not isIt() and id != roomData.itID()){
				alpha = Math::Map(alpha, 0.0, 1.0, 0.5, 1.0);
			}
			const Vec2& pos = localPlayer.pos;
			drawGoast(pos, alpha, id, player, localPlayer);
		}

		const Player& player = getPlayer();
		LocalPlayerID id = getLocalPlayerID();
		const PlayerLocalData& localPlayer = playersLocalData.at(id);
		double alpha = 1.0;
		if (player.isTransparent) {
			alpha = (localPlayer.fadeoutTimer.progress1_0() * 0.75 + 0.25);
		}
		else {
			alpha = (localPlayer.fadeoutTimer.progress0_1() * 0.75 + 0.25);
		}
		const Vec2& pos = playerBody.getPos();
		drawGoast(pos, alpha, id, player, localPlayer);
		
		if(tagStoppingTimer.isRunning()){
			FontAsset(U"message")(U"鬼ごっこ再開まで…", Ceil(tagStoppingTimer.sF()), U"秒").drawAt(Vec2{ 400,550 }, Palette::White);
		}

		//observerAccumulatedTime Circle
		Circle trapAccumulatedCircle{ Scene::Size() - Vec2{40,40},25 };
		trapAccumulatedCircle.draw(Palette::Dimgray);
		{
			ScopedRenderStates2D sampler{ SamplerState::ClampNearest };
			int32 page = static_cast<int32>(Scene::Time() / 0.25) % 4;
			HSV hsv(player.color);
			hsv.s = 0.5;
			TextureAsset(U"pow")(page % 2 * 32, page / 2 * 32, 32, 32).scaled(2).drawAt(trapAccumulatedCircle.center, hsv);
		}
		trapAccumulatedCircle.drawPie(0, trapAccumulatedTime / trapStepTime * Math::TwoPi, ColorF(1, 0.3)).drawFrame(3, Palette::Gray);
		if (player.isTransparent) {
			trapAccumulatedCircle.drawFrame(3, Palette::Red);
			Vec2 dir = Circular{ trapAccumulatedCircle.r, 45_deg };
			Line{ trapAccumulatedCircle.center - dir,trapAccumulatedCircle.center + dir }.draw(3, Palette::Red);
		}

		if (SimpleGUI::Button(U"Exit", Vec2{ 700,10 })) {
			leaveRoom();
			state = NetWorkState::Leaving;
		}
	}

	void connectReturn([[maybe_unused]] const int32 errorCode, const String& errorString, const String& region, [[maybe_unused]] const String& cluster) override
	{
		state = NetWorkState::InLobby;
		isFirstConnecting = false;
	}

	void disconnectReturn() override
	{
		state = NetWorkState::Disconnected;
	}

	void leaveRoomReturn(int32 errorCode, const String& errorString) override
	{
		state = NetWorkState::InLobby;
	}

	void joinRoomEventAction(const LocalPlayer& newPlayer, [[maybe_unused]] const Array<LocalPlayerID>& playerIDs, const bool isSelf) override {
		if (isSelf) {
			state = NetWorkState::InRoom;

			noMovingTime.restart();
			if (isHost()) {

				roomData.addPlayer(newPlayer.localID, Vec2{ 400,300 }, RandomColor(), userNameBox.text);
				playersLocalData.insert_or_assign(newPlayer.localID, PlayerLocalData{ Vec2{ 400,300 } });

				roomData.setItID(newPlayer.localID);
			}
			else {
			}
		}
		else {
			if (isHost()) {
				sendEvent({FromEnum(EventCode::roomDataFromHost), Array{ newPlayer.localID }}, Serializer<MemoryWriter>{}(roomData, trapAccumulatedTime));
			}
		}
	}

	void leaveRoomEventAction(const LocalPlayerID playerID, [[maybe_unused]] const bool isInactive) override {
		if (isHost()) {

			if(playerID == roomData.itID()){
				roomData.setItID(getLocalPlayerID());
				sendEvent(FromEnum(EventCode::itIDChange), Serializer<MemoryWriter>{}(getLocalPlayerID()));

				tagStoppingTimer.restart(3.0s);
				roomData.clearTraps();
				trapAccumulatedTime = 0.0;
				sendEvent(FromEnum(EventCode::tagStop), Serializer<MemoryWriter>{});
			}

			roomData.erasePlayer(playerID);
			playersLocalData.erase(playerID);
			roomData.eraseTrap(playerID);
			sendEvent(FromEnum(EventCode::playerErase), Serializer<MemoryWriter>{}(playerID));
		}
	}

	void customEventAction(const LocalPlayerID playerID, const uint8 eventCode, Deserializer<MemoryViewReader>& reader) override
	{
		auto eventCodeEnum = eventCode;
		switch (eventCodeEnum) {
		case EventCode::roomDataFromHost:
		{
			assert(not hasRoomData);
			reader(roomData, trapAccumulatedTime);
			hasRoomData = true;
			Vec2 pos = Vec2{ 400,300 };
			Color color = RandomColor();
			for(auto [id,player] : roomData.players()){
				playersLocalData.insert_or_assign(id, PlayerLocalData{ player.pos });
			}
			roomData.addPlayer(getLocalPlayerID(), pos, color, userNameBox.text);
			playersLocalData.insert_or_assign(getLocalPlayerID(), PlayerLocalData{ pos });
			sendEvent(FromEnum(EventCode::playerAdd), Serializer<MemoryWriter>{}(pos, color, userNameBox.text));
		}
			break;
		case EventCode::playerAdd:
		{
			if (not hasRoomData) return;
			Vec2 pos;
			Color color;
			String name;
			reader(pos, color, name);
			roomData.addPlayer(playerID, pos, color, name);
			playersLocalData.insert_or_assign(playerID, PlayerLocalData{ pos });
		}
			break;
		case EventCode::playerErase:
			if (not hasRoomData) return;
			LocalPlayerID erasePlayerID;
			reader(erasePlayerID);
			roomData.erasePlayer(erasePlayerID);
			playersLocalData.erase(erasePlayerID);
			roomData.eraseTrap(erasePlayerID);
			break;
		case EventCode::playerMove:
		{
			if (not hasRoomData) return;
			Vec2 pos;
			reader(pos);
			roomData.setPlayerPos(playerID, pos);
		}
			break;
		case EventCode::beTransparent:
		{
			if (not hasRoomData) return;
			bool beTransparent;
			reader(beTransparent);
			roomData.beTransparent(playerID, beTransparent);
			flipFadeoutTimer(playerID);
		}
			break;
		case EventCode::beWatching:
		{
			if (not hasRoomData) return;
			bool beWatching;
			reader(beWatching);
			roomData.beWatching(playerID, beWatching);
		}
			break;
		case EventCode::beSlowdown:
		{
			if (not hasRoomData) return;
			bool beSlowdown;
			reader(beSlowdown);
			roomData.beSlowdown(playerID, beSlowdown);
			if (beSlowdown)playersLocalData.at(playerID).slowdownTimer.restart(slowDownTime);
		}
			break;
		case EventCode::playerNameChange:
		{
			if (not hasRoomData) return;
			String name;
			reader(name);
			roomData.setPlayerName(playerID, name);
		}
			break;
		case EventCode::itIDChange:
		{
			if (not hasRoomData) return;
			LocalPlayerID itID;
			reader(itID);
			roomData.setItID(itID);
		}
			break;
		case EventCode::tagStop:
		{
			if (not hasRoomData) return;
			tagStoppingTimer.restart(3.0s);
			roomData.clearTraps();
			trapAccumulatedTime = 0.0;
		}
			break;
		case EventCode::addTrap:
		{
			if (not hasRoomData) return;
			Vec2 pos;
			Color color;
			reader(pos,color);
			roomData.addTrap(pos, playerID,color);
		}
			break;
		case EventCode::requestTrappedToHost:
		{
			if (not hasRoomData) return;

			if (isHost()) {
				size_t trapID;
				reader(trapID);
				if(roomData.traps().contains(trapID)){
					roomData.eraseTrap(trapID);
					sendEvent(FromEnum(EventCode::eraseTrap), Serializer<MemoryWriter>{}(trapID));
					sendEvent({FromEnum(EventCode::solveTrapped), Array{ playerID }}, Serializer<MemoryWriter>{});
				}
			}
		}
			break;
		case EventCode::solveTrapped:
		{
			if (not hasRoomData) return;

			roomData.beSlowdown(getLocalPlayerID(), true);
			playersLocalData.at(getLocalPlayerID()).slowdownTimer.restart(slowDownTime);
			sendEvent(FromEnum(EventCode::beSlowdown), Serializer<MemoryWriter>{}(true));
		}
			break;
		case EventCode::eraseTrap:
		{
			if (not hasRoomData) return;
			size_t trapID;
			reader(trapID);
			roomData.eraseTrap(trapID);
		}
			break;
		default:
			break;
		}

	}
};


void Main()
{
	Scene::SetBackground(Color{ 66, 57, 36 });

	FontAsset::Register(U"message", 30, Typeface::Bold);
	FontAsset::Register(U"name", 15, Typeface::Bold);
	TextureAsset::Register(U"goast_body", Resource(U"image/goast_body.png"));
	TextureAsset::Register(U"goast_eye", Resource(U"image/goast_eye.png"));
	TextureAsset::Register(U"gold_crown", Resource(U"image/gold_crown.png"));
	TextureAsset::Register(U"pow", Resource(U"image/pow.png"));
	TextureAsset::Register(U"boseki", Resource(U"image/boseki.png"));

	

	const std::string secretAppID{ SIV3D_OBFUSCATE(PHOTON_APP_ID) };

	MyNetwork network{ secretAppID, U"1.1", Verbose::No };

	while (System::Update())
	{
		ClearPrint();

		if(not network.isActive()){
			network.initWhenEnterLobby();
			network.connect(U"player", U"jp");
			network.state = NetWorkState::Connecting;
		}
		else {
			network.update();
		}

		switch (network.state)
		{
		case NetWorkState::Disconnected:
			break;
		case NetWorkState::Connecting:
			if (network.isFirstConnecting) {
				FontAsset(U"message")(U"接続しています...").drawAt(Scene::Center(), Palette::White);
			}
			else {
				FontAsset(U"message")(U"接続が切れました\n再接続しています...").drawAt(Scene::Center(), Palette::White);
			}
			break;
		case NetWorkState::InLobby:
			network.updateLobby();
			break;
		case NetWorkState::Joining:
			FontAsset(U"message")(U"入室中...").drawAt(Scene::Center(), Palette::White);
			break;
		case NetWorkState::InRoom:
			network.updateRoom();
			network.drawRoom();
			break;
		case NetWorkState::Leaving:
			FontAsset(U"message")(U"退室中...").drawAt(Scene::Center(), Palette::White);
			break;
		case NetWorkState::Disconnecting:
			FontAsset(U"message")(U"切断中...").drawAt(Scene::Center(), Palette::White);
			break;
		default:
			break;
		}

	}
}

//
// - Debug ビルド: プログラムの最適化を減らす代わりに、エラーやクラッシュ時に詳細な情報を得られます。
//
// - Release ビルド: 最大限の最適化でビルドします。
//
// - [デバッグ] メニュー → [デバッグの開始] でプログラムを実行すると、[出力] ウィンドウに詳細なログが表示され、エラーの原因を探せます。
//
// - Visual Studio を更新した直後は、プログラムのリビルド（[ビルド]メニュー → [ソリューションのリビルド]）が必要な場合があります。
//
