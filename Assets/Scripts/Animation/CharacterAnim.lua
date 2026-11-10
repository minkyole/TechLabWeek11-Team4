local locomotionPlayer

local function requireId(anim, id)
    -- Lua에서는 -1도 true이므로 assert(id)로 검사하면 안 된다.
    assert(id ~= -1, anim:GetLastError())
    return id
end

local function requireSuccess(anim, ok)
    assert(ok, anim:GetLastError())
end

function InitializeAnimation(anim)
    -- 재생 노드
    locomotionPlayer = requireId(
        anim,
        anim:CreateBlendSpacePlayer("Locomotion", {
            loop = true
        })
    )

    local attackPlayer = requireId(
        anim,
        anim:CreateSequencePlayer("Attack", {
            loop = false
        })
    )

    local hitPlayer = requireId(
        anim,
        anim:CreateSequencePlayer("Hit", {
            loop = false
        })
    )

    local jumpPlayer = requireId(
        anim,
        anim:CreateSequencePlayer("Jump", {
            loop = true
        })
    )

    -- 상태
    local locomotion = requireId(
        anim, anim:AddState("Locomotion", locomotionPlayer)
    )

    local attack = requireId(
        anim, anim:AddState("Attack", attackPlayer)
    )

    local hit = requireId(
        anim, anim:AddState("Hit", hitPlayer)
    )

    local jump = requireId(
        anim, anim:AddState("Jump", jumpPlayer)
    )

    requireSuccess(anim, anim:SetEntryState(locomotion))

    -- 파라미터
    requireSuccess(anim, anim:SetFloat("Speed", 0.0))
    requireSuccess(anim, anim:SetBool("IsGrounded", true))
    requireSuccess(anim, anim:DefineTrigger("AttackRequested"))
    requireSuccess(anim, anim:DefineTrigger("HitRequested"))

    -- 조건
    local canAttack = requireId(
        anim,
        anim:AddCondition(function(ctx)
            return ctx:GetBool("IsGrounded")
                and ctx:IsTriggerActive("AttackRequested")
        end)
    )

    local airborne = requireId(
        anim,
        anim:AddCondition(function(ctx)
            return not ctx:GetBool("IsGrounded")
        end)
    )

    local grounded = requireId(
        anim,
        anim:AddCondition(function(ctx)
            return ctx:GetBool("IsGrounded")
        end)
    )

    local hitRequested = requireId(
        anim,
        anim:AddCondition(function(ctx)
            return ctx:IsTriggerActive("HitRequested")
        end)
    )

    local finished = requireId(
        anim,
        anim:AddCondition(function(ctx)
            return ctx:IsSourceFinished()
        end)
    )

    -- 모든 상태 → 피격
    requireId(
        anim,
        anim:AddAnyStateTransition(hit, {
            condition = hitRequested,
            priority = 100,
            blendDuration = 0.1,
            restartIfSameState = true,
            interruptPolicy = "Always",
            consumeTriggers = { "HitRequested" }
        })
    )

    -- 이동 → 공격
    requireId(
        anim,
        anim:AddTransition(locomotion, attack, {
            condition = canAttack,
            priority = 50,
            blendDuration = 0.2,
            consumeTriggers = { "AttackRequested" }
        })
    )

    -- 공격 → 이동
    requireId(
        anim,
        anim:AddTransition(attack, locomotion, {
            condition = finished,
            blendDuration = 0.2
        })
    )

    -- 피격 → 이동
    requireId(
        anim,
        anim:AddTransition(hit, locomotion, {
            condition = finished,
            blendDuration = 0.2
        })
    )

    -- 이동 → 점프
    -- 이동 중 공중에 있으면 Jump 상태로 진입
    -- 점프뿐 아니라 절벽에서 떨어지는 경우도 포함한다.
    requireId(
        anim,
        anim:AddTransition(locomotion, jump, {
            condition = airborne,
            priority = 60,
            blendDuration = 0.1,
        })
    )

    -- 점프 → 이동
    -- 클립 종료가 아니라 실제 착지를 기준으로 복귀
    requireId(
        anim,
        anim:AddTransition(jump, locomotion, {
            condition = grounded,
            blendDuration = 0.1
        })
    )

end

function UpdateAnimation(anim, deltaTime)
    local movement = anim:GetMovementComponent()

    local speed = 0.0
    local verticalSpeed = 0.0
    local grounded = false

    if movement ~= nil then
        speed = movement:GetGroundSpeed()
        verticalSpeed = movement:GetVerticalVelocity()
        grounded = movement:IsGrounded()
    end

    requireSuccess(anim, anim:SetFloat("Speed", speed))
    requireSuccess(anim, anim:SetFloat("VerticalSpeed", verticalSpeed))
    requireSuccess(anim, anim:SetBool("IsGrounded", grounded))

    requireSuccess(
        anim,
        anim:SetBlendSpaceInput(locomotionPlayer, speed)
    )
end