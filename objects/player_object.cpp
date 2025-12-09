#include "player_object.h"

<<<<<<< Updated upstream
// Ã¿Ö¡ÒÆ¶¯µÄËÙ¶È
static constexpr float speed = 3.0f;
const float PI = 3.14159265358979f;

// ÎÄ¼ş¼¶Ó³Éä£ºÎªÃ¿¸ö PlayerObject ÊµÀı±£´æÊÇ·ñ×ÅµØ
static std::unordered_map<const PlayerObject*, bool> s_grounded_map;

// ÎÄ¼ş¼¶Ó³Éä£ºÎªÃ¿¸öÊµÀı±£´æ°´×¡ÌøÔ¾¿ÉÓÃµÄÊ£ÓàÖ¡Êı£¨variable jump£©
static std::unordered_map<const PlayerObject*, float> s_jump_hold_time_left;

// ÎÄ¼ş¼¶Ó³Éä£ºÎªÃ¿¸öÊµÀı±£´æÀë¿ªµØÃæºóÔÊĞíÌøÔ¾µÄ coyote Ê±¼ä£¨Ö¡£©
static std::unordered_map<const PlayerObject*, float> s_coyote_time_left;

// ¿Éµ÷²ÎÊı
static constexpr int max_jump_hold_frames = 12;       // °´×¡ÌøÔ¾×î¶àÔÊĞíµÄÖ¡Êı£¨°´×¡Ô½¾ÃÌøµÃÔ½¸ß£©
static constexpr float low_gravity_multiplier = 0.4f; // °´×¡ÌøÔ¾Ê±µÄÖØÁ¦±¶ÂÊ£¨½ÏĞ¡£¬±£ÁôÉÏÉıËÙ¶È£©
static constexpr float fall_gravity_multiplier = 1.6f; // ËÉ¿ª»òÏÂÂäÊ±µÄ¼ÓÇ¿ÖØÁ¦±¶ÂÊ£¨¸ü¿ìÏÂÂä£©
static constexpr int coyote_time_frames = 6;         // ÀëµØºóÈÔ¿ÉÌøµÄÖ¡Êı£¨coyote time£©

void PlayerObject::Start()
{
	// Í³Ò»ÉèÖÃÌùÍ¼Â·¾¶¡¢ÊúÅÅÖ¡Êı¡¢¶¯»­¸üĞÂÆµÂÊºÍ»æÖÆÉî¶È£¬²¢×¢²áµ½»æÖÆĞòÁĞ
	// ÈçĞèÒªÄ¬ÈÏÖµ£¬ÇëÊ¹ÓÃ¸ßÁ£¶ÈµÄ SetSprite*() ºÍ Set*() ·½·¨ÖğÒ»ÉèÖÃ·ÇÄ¬ÈÏÖµ²ÎÊı
	// ×ÊÔ´Â·¾¶ÎŞÄ¬ÈÏÖµ£¬±ØĞëÊÖ¶¯ÉèÖÃ
=======

// ä¸ºæ¯ä¸ª PlayerObject å®ä¾‹ä¿å­˜æ˜¯å¦ç€åœ°
static std::unordered_map<const PlayerObject*, bool> s_grounded_map;

// ä¸ºæ¯ä¸ªå®ä¾‹ä¿å­˜æŒ‰ä½è·³è·ƒå¯ç”¨çš„å‰©ä½™å¸§æ•°ï¼ˆvariable jumpï¼‰
static std::unordered_map<const PlayerObject*, float> s_jump_hold_time_left;

// ä¸ºæ¯ä¸ªå®ä¾‹ä¿å­˜ç¦»å¼€åœ°é¢åå…è®¸è·³è·ƒçš„ coyote æ—¶é—´ï¼ˆå¸§ï¼‰
static std::unordered_map<const PlayerObject*, float> s_coyote_time_left;

// å¯è°ƒå‚æ•°
static constexpr int max_jump_hold_frames = 12;       // æŒ‰ä½è·³è·ƒæœ€å¤šå…è®¸çš„å¸§æ•°ï¼ˆæŒ‰ä½è¶Šä¹…è·³å¾—è¶Šé«˜ï¼‰
static constexpr float low_gravity_multiplier = 0.4f; // æŒ‰ä½è·³è·ƒæ—¶çš„é‡åŠ›å€ç‡ï¼ˆè¾ƒå°ï¼Œä¿ç•™ä¸Šå‡é€Ÿåº¦ï¼‰
static constexpr float fall_gravity_multiplier = 1.6f; // æ¾å¼€æˆ–ä¸‹è½æ—¶çš„åŠ å¼ºé‡åŠ›å€ç‡ï¼ˆæ›´å¿«ä¸‹è½ï¼‰
static constexpr int coyote_time_frames = 6;         // ç¦»åœ°åä»å¯è·³çš„å¸§æ•°ï¼ˆcoyote timeï¼‰

// æ¯å¸§ç§»åŠ¨çš„é€Ÿåº¦
static constexpr float speed = 3.0f;
const float PI = 3.14159265358979f;
ObjToken test[3] = { ObjToken::Invalid() };
int i = 0;

static constexpr float gravity = 3.0f;        // åŸºç¡€æ¯å¸§é‡åŠ›åŠ é€Ÿåº¦ï¼ˆå¯æ ¹æ®éœ€è¦è°ƒæ•´ï¼‰
static constexpr float max_fall_speed = -12.0f; // ç»ˆç«¯ä¸‹è½é€Ÿåº¦ï¼ˆè´Ÿå€¼ï¼‰

void PlayerObject::Start()
{
    // ç»Ÿä¸€è®¾ç½®è´´å›¾è·¯å¾„ã€ç«–æ’å¸§æ•°ã€åŠ¨ç”»æ›´æ–°é¢‘ç‡å’Œç»˜åˆ¶æ·±åº¦ï¼Œå¹¶æ³¨å†Œåˆ°ç»˜åˆ¶åºåˆ—
    // å¦‚éœ€è¦é»˜è®¤å€¼ï¼Œè¯·ä½¿ç”¨é«˜ç²’åº¦çš„ SetSprite*() å’Œ Set*() æ–¹æ³•é€ä¸€è®¾ç½®éé»˜è®¤å€¼å‚æ•°
    // èµ„æºè·¯å¾„æ— é»˜è®¤å€¼ï¼Œå¿…é¡»æ‰‹åŠ¨è®¾ç½®
>>>>>>> Stashed changes
    SpriteSetStats("/sprites/idle.png", 3, 7, 0);

    // ¿ÉÑ¡£º³õÊ¼»¯Î»ÖÃ£¨¸ù¾İĞèÒªµ÷Õû£©£¬ÀıÈçÆÁÄ»ÖĞĞÄ¸½½ü
    SetPosition(cf_v2(0.0f, 0.0f));

    Scale(0.5f);
<<<<<<< Updated upstream
	SetPivot(1,1);

	// ³õÊ¼»¯×ÅµØ×´Ì¬Îª false
	s_grounded_map[this] = false;
	// ³õÊ¼»¯°´×¡ÌøÔ¾±£ÁôÓë coyote Ê±¼äÎª 0
	s_jump_hold_time_left[this] = 0.0f;
	s_coyote_time_left[this] = 0.0f;
=======
    SetPivot(1,1);

    // ç¡®ä¿ maps æœ‰é»˜è®¤æ¡ç›®ï¼ˆå¯é€‰ï¼‰
    s_grounded_map[this] = false;
    s_jump_hold_time_left[this] = 0.0f;
    s_coyote_time_left[this] = 0.0f;
>>>>>>> Stashed changes
}

void PlayerObject::Update()
{
<<<<<<< Updated upstream
	// µ±¼ì²âµ½°´¼ü°´ÏÂÊ±£¬ÉèÖÃËÙ¶È·½Ïò£¨²»Ö±½Ó SetPosition£¬Ê¹ÓÃËÙ¶È»ı·Ö£©
    CF_V2 dir = cf_v2(0,0);
    if (cf_key_down(CF_KEY_A)) {
        dir.x -= 1;
    }
    if (cf_key_down(CF_KEY_D)) {
		dir.x += 1;
    }
    if (cf_key_down(CF_KEY_S)) {
		dir.y -= 1;
=======
    // å½“æ£€æµ‹åˆ°æŒ‰é”®æŒ‰ä¸‹æ—¶ï¼Œè®¾ç½®é€Ÿåº¦æ–¹å‘ï¼ˆä¸ç›´æ¥ SetPositionï¼Œä½¿ç”¨é€Ÿåº¦ç§¯åˆ†ï¼‰
    CF_V2 dir(0,0);
    if (Input::IsKeyInState(CF_KEY_A, KeyState::Hold)) {
        dir.x -= 1;
    }
    if (Input::IsKeyInState(CF_KEY_D, KeyState::Hold)) {
        dir.x += 1;
    }
    if (Input::IsKeyInState(CF_KEY_SPACE, KeyState::Hold)) {
        dir.y += 1;
    }
    if (Input::IsKeyInState(CF_KEY_S, KeyState::Hold)) {
        dir.y -= 1;
>>>>>>> Stashed changes
    }

    // ¶ÁÈ¡µ±Ç°ÊµÀıµÄ×ÅµØÓë¼ÆÊ±Æ÷×´Ì¬
    bool grounded = false;
    auto itg = s_grounded_map.find(this);
    if (itg != s_grounded_map.end()) grounded = itg->second;

    float hold_time_left = 0.0f;
    auto ith = s_jump_hold_time_left.find(this);
    if (ith != s_jump_hold_time_left.end()) hold_time_left = ith->second;

    float coyote_left = 0.0f;
    auto itc = s_coyote_time_left.find(this);
    if (itc != s_coyote_time_left.end()) coyote_left = itc->second;

    // ÌøÔ¾´¦Àí£ºÖ§³Ö coyote time£¨ÀëµØ¶ÌÊ±¼äÄÚÈÔ¿ÉÌø£©ÒÔ¼°°´×¡ÑÓ³¤ÌøÔ¾£¨Í¨¹ı½µµÍÖØÁ¦£©
    static constexpr float jump_speed = 20.0f;
    bool space_down = cf_key_down(CF_KEY_SPACE);

    // Èç¹ûÂú×ãÌøÔ¾Ìõ¼ş£¨×ÅµØ»òÔÚ coyote ´°¿ÚÄÚ£©£¬²¢ÇÒ°´¼ü´¦ÓÚ°´ÏÂ×´Ì¬£¬ÔòÆğÌø
    if (space_down && (grounded || coyote_left > 0.0f)) {
        // ÆğÌø£ºÉèÖÃ³õËÙ¶È²¢¿ªÆô°´×¡±£Áô´°¿Ú
        CF_V2 cur_vel = GetVelocity();
        cur_vel.y = jump_speed;
        SetVelocity(cur_vel);

        // ÆğÌøºóÇå³ı×ÅµØºÍ coyote ×´Ì¬£¬Æô¶¯°´×¡ÌøÔ¾¼ÆÊ±
        s_grounded_map[this] = false;
        s_coyote_time_left[this] = 0.0f;
        s_jump_hold_time_left[this] = static_cast<float>(max_jump_hold_frames);
        hold_time_left = static_cast<float>(max_jump_hold_frames);
    }

    // Èç¹û°´×¡¿Õ¸ñ²¢ÇÒ»¹ÓĞ°´×¡Ê±¼ä£¬Ôò±£Áô°´×¡Ê±¼ä£¨ÔÚÏÂÃæÓÃÓÚ½µµÍÖØÁ¦£©
    if (space_down && hold_time_left > 0.0f) {
        // Ã¿Ö¡ÏûºÄÒ»Ö¡µÄ°´×¡Ê±¼ä
        hold_time_left -= 1.0f;
        s_jump_hold_time_left[this] = hold_time_left;
    } else if (!space_down && hold_time_left > 0.0f) {
        // ËÉ¿ªÊ±È¡ÏûÊ£Óà°´×¡Ê±¼ä
        s_jump_hold_time_left[this] = 0.0f;
        hold_time_left = 0.0f;
    }

    // Ë®Æ½ËÙ¶È£ºÖ»ĞŞ¸Ä x£¬±£Áô y£¨ÒÔÃâ¸²¸ÇÌøÔ¾»òÖØÁ¦Ôì³ÉµÄËÙ¶È£©
    CF_V2 cur_vel = GetVelocity();
    cur_vel.x = dir.x * speed;
    SetVelocity(cur_vel);

    // ¸ù¾İË®Æ½ÒÆ¶¯·½Ïò·­×ªÌùÍ¼²¢ÇĞ»»¶¯»­
    if (dir.x != 0) {
<<<<<<< Updated upstream
		SpriteFlipX(dir.x < 0); // ¸ù¾İË®Æ½ÒÆ¶¯·½Ïò·­×ªÌùÍ¼
=======
        SpriteFlipX(dir.x < 0); // æ ¹æ®æ°´å¹³ç§»åŠ¨æ–¹å‘ç¿»è½¬è´´å›¾
>>>>>>> Stashed changes
        SpriteSetStats("/sprites/walk.png", 2, 7, 0, false);
    }
    else {
        SpriteSetStats("/sprites/idle.png", 3, 7, 0, false);
    }
<<<<<<< Updated upstream
=======
    // å½’ä¸€åŒ–é€Ÿåº¦å‘é‡ä»¥é˜²å¯¹è§’ç§»åŠ¨è¿‡å¿«
    dir = v2math::normalized(dir);
    CF_V2 vel = cf_v2(dir.x * speed, dir.y * speed);
>>>>>>> Stashed changes

	// Îª Player ¶ÔÏóÌí¼ÓÊ¼ÖÕÏò y Öá¸º·½ÏòµÄÖØÁ¦£¬µ«¸ù¾İ×´Ì¬Ê¹ÓÃ²»Í¬±¶ÂÊ£º
    // - ÔÚ°´×¡ÌøÔ¾ÇÒ´¦ÓÚ°´×¡´°¿ÚÊ±£º½µµÍÖØÁ¦£¨ÀıÈç³ËÒÔ 0.4£©£¬ÈÃÉÏÉı¸ü¸ß
    // - ÔÚÏÂÂäÊ±£¨ËÙ¶ÈÎª¸º£©£ºÊ¹ÓÃ¼ÓÇ¿ÖØÁ¦±¶ÂÊ£¬ÈÃÏÂÂä¸üÔúÊµ£¨¸ü¿ì£©
    // - ÆäËüÇé¿öÊ¹ÓÃ±ê×¼ÖØÁ¦
    static constexpr float gravity = 3.0f;        // »ù´¡Ã¿Ö¡ÖØÁ¦¼ÓËÙ¶È£¨¿É¸ù¾İĞèÒªµ÷Õû£©
    static constexpr float max_fall_speed = -12.0f; // ÖÕ¶ËÏÂÂäËÙ¶È£¨¸ºÖµ£©

<<<<<<< Updated upstream
    // ¶ÁÈ¡µ±Ç°´¹Ö±ËÙ¶ÈÒÔÅĞ¶ÏÊÇÉÏÉı»¹ÊÇÏÂÂä
    cur_vel = GetVelocity();
    float gravity_multiplier = 1.0f;

    // ½öÔÚÉÏÉı½×¶Î£¨´¹Ö±ËÙ¶ÈÎªÕı£©²¢ÇÒÕıÔÚ°´×¡ÇÒÓĞ±£ÁôÊ±¼äÊ±Ê¹ÓÃµÍÖØÁ¦
    if (cur_vel.y > 0.0f && space_down && hold_time_left > 0.0f) {
        gravity_multiplier = low_gravity_multiplier;
    } else if (cur_vel.y < 0.0f) {
        // ÏÂÂä½×¶ÎÊ¹ÓÃ¼ÓÇ¿ÖØÁ¦
        gravity_multiplier = fall_gravity_multiplier;
    } else {
        gravity_multiplier = 1.0f;
    }

    // Ó¦ÓÃÖØÁ¦
    AddVelocity(cf_v2(0.0f, -gravity * gravity_multiplier));

    // ÏŞÖÆÏÂÂäËÙ¶È£¨·ÀÖ¹ÎŞÏŞ¼ÓËÙ£©
    cur_vel = GetVelocity();
    if (cur_vel.y < max_fall_speed) {
        cur_vel.y = max_fall_speed;
        SetVelocity(cur_vel);
    }

    // Ã¿Ö¡µİ¼õ coyote Ê±¼ä£¨ÈôÓĞ£©
    if (coyote_left > 0.0f) {
        coyote_left -= 1.0f;
        s_coyote_time_left[this] = coyote_left;
    }

	// ¼ÆËã³¯Ïò½Ç¶È£¨»¡¶ÈÖÆ£¬0 ¶ÈÎªÕıÓÒ£¬ÄæÊ±ÕëĞı×ª£©
    float angle = 0;
    if (cf_key_down(CF_KEY_Q)) {
		angle += PI / 60.0f; // Ã¿Ö¡ÄæÊ±ÕëĞı×ª 3 ¶È
    }
    if (cf_key_down(CF_KEY_E)) {
		angle -= PI / 60.0f; // Ã¿Ö¡Ë³Ê±ÕëĞı×ª 3 ¶È
    }
	Rotate(angle);
}

void PlayerObject::OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    // Åö×²½øÈëÊ±µÄ´¦ÀíÂß¼­
    printf("Collided with object token: %u\n", other_token.index);

	// ½«Player¶ÔÏóµÄÎ»ÖÃÖØÖÃµ½ÉÏÒ»Ö¡Î»ÖÃ£¬±ÜÃâ´©Í¸
	SetPosition(GetPrevPosition());

	// ÅĞ¶ÏÊÇ·ñÎª¡°µØÃæ½Ó´¥¡±£ºÊ¹ÓÃĞŞÕıÏòÁ¿µÄ y ·ÖÁ¿£¨correction = -n * depth£©
	// Èç¹û correction.y > 0 ÔòËµÃ÷±»ÏòÉÏĞŞÕı£¨Õ¾ÔÚ±ğµÄÎïÌåÉÏ£©
	if (manifold.count > 0) {
		float correction_y = -manifold.n.y * manifold.depths[0];
		if (correction_y > 0.001f) {
			s_grounded_map[this] = true;
			// ×ÅµØÊ±È¡Ïû°´×¡ÌøÔ¾±£ÁôÊ±¼äÓë coyote Ê±¼ä
			s_jump_hold_time_left[this] = 0.0f;
			s_coyote_time_left[this] = 0.0f;
		}
	}
}

void PlayerObject::OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    // Åö×²³ÖĞøÊ±µÄ´¦ÀíÂß¼­
	printf("Still colliding with object token: %u\n", other_token.index);

	CF_V2 correction = cf_v2(-manifold.n.x * manifold.depths[0], -manifold.n.y * manifold.depths[0]);
	CF_V2 current_position = GetPosition(); // Ê¹ÓÃ¹«¿ª½Ó¿Ú
	CF_V2 new_position = cf_v2(current_position.x + correction.x, current_position.y + correction.y);
	SetPosition(new_position);

	// Èç¹ûĞŞÕıÏòÁ¿ÓĞÕıµÄ y ·ÖÁ¿£¬ËµÃ÷ÎÒÃÇ±»ÏòÉÏÍÆ£¬ÊÓÎª×ÅµØ
	if (correction.y > 0.001f) {
		s_grounded_map[this] = true;
		// ×ÅµØÊ±È¡Ïû°´×¡ÌøÔ¾±£ÁôÊ±¼äÓë coyote Ê±¼ä
		s_jump_hold_time_left[this] = 0.0f;
		s_coyote_time_left[this] = 0.0f;
	}
}

void PlayerObject::OnCollisionExit(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    // Åö×²ÍË³öÊ±µÄ´¦ÀíÂß¼­
    printf("No longer colliding with object token: %u\n", other_token.index);

	// Àë¿ªÅö×²Ê±È¡Ïû×ÅµØ±ê¼Ç£¬²¢Æô¶¯ coyote Ê±¼ä£¨¶ÌÔİÔÊĞíÔÙ´ÎÌøÔ¾£©
	s_grounded_map[this] = false;
	s_coyote_time_left[this] = static_cast<float>(coyote_time_frames);
	// Àë¿ªÊ±²»±£Áô°´×¡Ê±¼ä£¨°´×¡ÌøÔ¾½öÔÚÆğÌøºó¶ÌÊ±¼äÄÚÓĞĞ§£©
	s_jump_hold_time_left[this] = 0.0f;
=======
    // è®¡ç®—æœå‘è§’åº¦ï¼ˆå¼§åº¦åˆ¶ï¼Œ0 åº¦ä¸ºæ­£å³ï¼Œé€†æ—¶é’ˆæ—‹è½¬ï¼‰
    float angle = 0;
    if (Input::IsKeyInState(CF_KEY_Q, KeyState::Hold)) {
        angle += PI / 60.0f; // æ¯å¸§é€†æ—¶é’ˆæ—‹è½¬ 3 åº¦
        std::cout << GetRotation() << std::endl;
    }
    if (Input::IsKeyInState(CF_KEY_E, KeyState::Hold)) {
        angle -= PI / 60.0f; // æ¯å¸§é¡ºæ—¶é’ˆæ—‹è½¬ 3 åº¦
    }
    Rotate(angle);

    // æŒ‰ W é”®å‘å°„ TestObject å®ä¾‹
    if (Input::IsKeyInState(CF_KEY_W, KeyState::Down)) {
        if (objs.TryGetRegisteration(test[i])) {
            objs.Destroy(test[i]);
        }
        auto test_token = objs.Create<TestObject>();
        if (test_token.isValid()) test[i] = test_token;
        auto rot = GetRotation();
        int flip = (SpriteGetFlipX() ? -1 : 1);
        objs[test[i]].SetRotation(rot);
        objs[test[i]].SpriteFlipX(SpriteGetFlipX());
        objs[test[i]].SetPosition(GetPosition());
        objs[test[i]].SetVisible(true);
        objs[test[i]].SetVelocity(v2math::angled(CF_V2(30.0f), rot) * flip);
        i = (i + 1) % 3; // åœºä¸Šä»…å­˜åœ¨3ä¸ª TestObject å®ä¾‹ï¼Œè‹¥å¤šå‡ºåˆ™é”€æ¯æœ€æ—©ç”Ÿæˆçš„é‚£ä¸ª
    }

    // è¯»å–å½“å‰å‚ç›´é€Ÿåº¦ä»¥åˆ¤æ–­æ˜¯ä¸Šå‡è¿˜æ˜¯ä¸‹è½
    CF_V2 cur_vel = GetVelocity();
    float gravity_multiplier = 1.0f;

    // è¯»å–æŒ‰ä½è·³è·ƒç›¸å…³çŠ¶æ€ä¸ä¿ç•™æ—¶é—´ï¼ˆmap ä¸­å¯èƒ½æ²¡æœ‰æ¡ç›®åˆ™ operator[] ä¼šæ’å…¥é»˜è®¤å€¼ï¼‰
    bool space_down = Input::IsKeyInState(CF_KEY_SPACE, KeyState::Hold);
    float hold_time_left = s_jump_hold_time_left[this];
    float coyote_left = s_coyote_time_left[this];

    // ä»…åœ¨ä¸Šå‡é˜¶æ®µï¼ˆå‚ç›´é€Ÿåº¦ä¸ºæ­£ï¼‰å¹¶ä¸”æ­£åœ¨æŒ‰ä½ä¸”æœ‰ä¿ç•™æ—¶é—´æ—¶ä½¿ç”¨ä½é‡åŠ›
    if (cur_vel.y > 0.0f && space_down && hold_time_left > 0.0f) {
        gravity_multiplier = low_gravity_multiplier;
    }
    else if (cur_vel.y < 0.0f) {
        // ä¸‹è½é˜¶æ®µä½¿ç”¨åŠ å¼ºé‡åŠ›
        gravity_multiplier = fall_gravity_multiplier;
    }
    else {
        gravity_multiplier = 1.0f;
    }

    // åº”ç”¨é‡åŠ›ï¼ˆå‘ y è´Ÿæ–¹å‘æ–½åŠ ï¼‰
    AddVelocity(cf_v2(0.0f, -gravity * gravity_multiplier));

    // é™åˆ¶ä¸‹è½é€Ÿåº¦ï¼ˆé˜²æ­¢æ— é™åŠ é€Ÿï¼‰
    cur_vel = GetVelocity();
    if (cur_vel.y < max_fall_speed) {
        cur_vel.y = max_fall_speed;
        SetVelocity(cur_vel);
    }

    // æ¯å¸§é€’å‡ coyote æ—¶é—´ï¼ˆè‹¥æœ‰ï¼‰
    if (coyote_left > 0.0f) {
        coyote_left -= 1.0f;
        if (coyote_left < 0.0f) coyote_left = 0.0f;
        s_coyote_time_left[this] = coyote_left;
    }

    // æ¯å¸§é€’å‡ jump hold æ—¶é—´ï¼ˆè‹¥æœ‰ï¼‰
    if (hold_time_left > 0.0f) {
        hold_time_left -= 1.0f;
        if (hold_time_left < 0.0f) hold_time_left = 0.0f;
        s_jump_hold_time_left[this] = hold_time_left;
    }
}

void PlayerObject::OnCollisionEnter(const ObjManager::ObjToken & other_token, const CF_Manifold & manifold) noexcept {
    // ç¢°æ’è¿›å…¥æ—¶çš„å¤„ç†é€»è¾‘
    printf("Collided with object token: %u\n", other_token.index);

    // å°†Playerå¯¹è±¡çš„ä½ç½®é‡ç½®åˆ°ä¸Šä¸€å¸§ä½ç½®ï¼Œé¿å…ç©¿é€
    SetPosition(GetPrevPosition());

    // åˆ¤æ–­æ˜¯å¦ä¸ºâ€œåœ°é¢æ¥è§¦â€ï¼šä½¿ç”¨ä¿®æ­£å‘é‡çš„ y åˆ†é‡ï¼ˆcorrection = -n * depthï¼‰
    // å¦‚æœ correction.y > 0 åˆ™è¯´æ˜è¢«å‘ä¸Šä¿®æ­£ï¼ˆç«™åœ¨åˆ«çš„ç‰©ä½“ä¸Šï¼‰
    if (manifold.count > 0) {
        float correction_y = -manifold.n.y * manifold.depths[0];
        if (correction_y > 0.001f) {
            s_grounded_map[this] = true;
            // ç€åœ°æ—¶å–æ¶ˆæŒ‰ä½è·³è·ƒä¿ç•™æ—¶é—´ä¸ coyote æ—¶é—´
            s_jump_hold_time_left[this] = 0.0f;
            s_coyote_time_left[this] = 0.0f;
        }
    }
}

void PlayerObject::OnCollisionStay(const ObjManager::ObjToken & other_token, const CF_Manifold & manifold) noexcept {
    // ç¢°æ’æŒç»­æ—¶çš„å¤„ç†é€»è¾‘
    printf("Still colliding with object token: %u\n", other_token.index);

    CF_V2 correction = cf_v2(-manifold.n.x * manifold.depths[0], -manifold.n.y * manifold.depths[0]);
    CF_V2 current_position = GetPosition(); // ä½¿ç”¨å…¬å¼€æ¥å£
    CF_V2 new_position = cf_v2(current_position.x + correction.x, current_position.y + correction.y);
    SetPosition(new_position);

    // å¦‚æœä¿®æ­£å‘é‡æœ‰æ­£çš„ y åˆ†é‡ï¼Œè¯´æ˜æˆ‘ä»¬è¢«å‘ä¸Šæ¨ï¼Œè§†ä¸ºç€åœ°
    if (correction.y > 0.001f) {
        s_grounded_map[this] = true;
        // ç€åœ°æ—¶å–æ¶ˆæŒ‰ä½è·³è·ƒä¿ç•™æ—¶é—´ä¸ coyote æ—¶é—´
        s_jump_hold_time_left[this] = 0.0f;
        s_coyote_time_left[this] = 0.0f;
    }
}

void PlayerObject::OnCollisionExit(const ObjManager::ObjToken & other_token, const CF_Manifold & manifold) noexcept {
    // ç¢°æ’é€€å‡ºæ—¶çš„å¤„ç†é€»è¾‘
    printf("No longer colliding with object token: %u\n", other_token.index);

    // ç¦»å¼€ç¢°æ’æ—¶å–æ¶ˆç€åœ°æ ‡è®°ï¼Œå¹¶å¯åŠ¨ coyote æ—¶é—´ï¼ˆçŸ­æš‚å…è®¸å†æ¬¡è·³è·ƒï¼‰
    s_grounded_map[this] = false;
    s_coyote_time_left[this] = static_cast<float>(coyote_time_frames);
    // ç¦»å¼€æ—¶ä¸ä¿ç•™æŒ‰ä½æ—¶é—´ï¼ˆæŒ‰ä½è·³è·ƒä»…åœ¨èµ·è·³åçŸ­æ—¶é—´å†…æœ‰æ•ˆï¼‰
    s_jump_hold_time_left[this] = 0.0f;
>>>>>>> Stashed changes
}