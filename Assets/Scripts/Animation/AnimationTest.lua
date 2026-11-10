function BeginPlay()
print("BeginPlay!\n")
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

function Tick(dt)
	-- print("Tick!\n")

	-- 메시 교체 등으로 AnimInstance가 재생성될 수 있으므로
	-- 장기간 저장하지 않고 사용할 때 조회한다.
	local anim = self:GetAnimInstance()

	if anim == nil or not anim:IsGraphReady() then
		return
	end

	-- 이동 애니메이션 테스트 입력
	local animSpeed = anim:GetFloat("Speed")
	local movingSpeed = 100.0

	if IsKeyDown(Key.W) then
		animSpeed = animSpeed + movingSpeed * dt
	end
	if IsKeyDown(Key.S) then
		animSpeed = animSpeed - movingSpeed * dt
	end

	-- Speed Cliping
	if animSpeed < 0.0 then
		animSpeed = 0.0
	elseif animSpeed > 600.0 then
		animSpeed = 600.0
	end

	anim:SetFloat("Speed", animSpeed)

	-- 누른 순간 한 번만 공격 요청
	if IsKeyPressed(Key.Space) then
		assert(
			anim:SetTrigger("AttackRequested"),
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
