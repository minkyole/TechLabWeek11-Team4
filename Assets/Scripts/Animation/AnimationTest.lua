function BeginPlay()
print("BeginPlay!\n")
	local camera = self:GetCamera()
	local manager = GetPlayerCameraManager()

	if camera ~= nil and manager ~= nil then
		assert(manager:SetViewTargetCamera(camera))
	end
end

function EndPlay()
print("EndPlay!\n")
end

function OnOverlap(OtherActor)
print("OnOverlap!\n")
end

-- Shape의 BeginOverlap은 기존 OnOverlap으로 전달됩니다.
function OnEndOverlap(OtherActor)
print("OnEndOverlap!\n")
end

-- Hit는 Sweep 이동으로 발생하며 Hit 정보는 Lua에 복사되어 전달됩니다.
function OnHit(OtherActor, Hit)
print("OnHit!", Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z)
end

local walkSpeed = 5.0
local runSpeed = 10.0

function Tick(dt)
	-- print("Tick!\n")
	local movement = self:GetMovementComponent()

    if movement == nil then
        return
    end

    local x = 0.0
    local y = 0.0

    if IsKeyDown(Key.W) then x = x + 1.0 end
    if IsKeyDown(Key.S) then x = x - 1.0 end
    if IsKeyDown(Key.D) then y = y + 1.0 end
    if IsKeyDown(Key.A) then y = y - 1.0 end

    local yaw = math.rad(self:GetActorRotation().Yaw)
    local cosYaw = math.cos(yaw)
    local sinYaw = math.sin(yaw)

    local worldDirection = FVector.new(
        x * cosYaw - y * sinYaw,
        x * sinYaw + y * cosYaw,
        0.0
    )

    movement:SetMaxSpeed(
        IsKeyDown(Key.Shift) and runSpeed or walkSpeed
    )

    -- 입력이 없을 때도 (0, 0, 0)을 전달한다.
    -- 대각선 정규화는 C++ SetMoveInput에서 처리한다.
    movement:SetMoveInput(worldDirection)

	-- 메시 교체 등으로 AnimInstance가 재생성될 수 있으므로
	-- 장기간 저장하지 않고 사용할 때 조회한다.
	local anim = self:GetAnimInstance()

	if anim == nil or not anim:IsGraphReady() then
		return
	end

	-- 누른 순간 한 번만 공격 요청
	if IsKeyPressed(Key.J) then
		assert(
			anim:SetTrigger("AttackRequested"),
			anim:GetLastError()
		)
	end

	-- 누른 순간 한 번만 점프 요청
	if IsKeyPressed(Key.Space) then
		assert(
			anim:SetTrigger("JumpRequested"),
			anim:GetLastError()
		)
	end

	-- 피격 테스트
	if IsKeyPressed(Key.H) then
		assert(
			anim:SetTrigger("HitRequested"),
			anim:GetLastError()
		)
	end
end

function OnReload()
print("Reload!\n")
end
