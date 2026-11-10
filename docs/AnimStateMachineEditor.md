# State Machine Editor

## Create and edit

1. In Content Drawer, right-click the Assets folder where the file should live, then choose **Create > Animation > State Machine**.
2. Select the skeletal mesh and set an `.animsm` save path under Assets.
3. Add states and select a Sequence or BlendSpace for each one. Use **Set as Entry** for the starting state.
4. In Parameters, declare Bool, Float and Trigger values. For a BlendSpace state, bind **Axis parameter** to a declared Float.
5. Select a source state, click **Link selected**, then click the destination state. **AnyState link** creates a transition from any state. The small arrow marker selects a transition.
6. Edit its condition index and transition settings. A new link initially uses an Always condition; change that condition before testing if an unconditional transition is not intended.
7. Conditions are listed by index. AND/OR/NOT reference earlier conditions; NOT requires exactly one child. Referenced conditions cannot be deleted until their references are removed.
8. Save. Double-click the `.animsm` file in Content Drawer to reopen it.

Drag a state to move it, middle-drag to pan, and use the wheel to zoom. Delete removes the selection; Ctrl+S saves. Removing a state also removes its transitions. Removing Entry requires choosing a new Entry. Undo/Redo is not included.

## Preview

Expand Preview and click **Apply to 3D Preview**. This opens Skeletal Mesh Viewer using a private snapshot of the working data, including unsaved edits. Change the preview's Bool/Float values or press a Trigger button to test transitions. Play/Pause and Reset control this instance. Current state and active transitions are highlighted.

Lua updates are disabled in preview. Apply again after editing to rebuild the preview. Sequence and BlendSpace playback windows are hidden while state machine preview owns the viewer. Opening another mesh or closing the state machine editor returns the viewer to ordinary animation preview.

## Apply to a character

Set the skeletal mesh component's **StateMachineAssetPath** to the file path, for example `Assets/Animation/Character.animsm`. It must use the same mesh as the state machine. The path persists with the scene and component duplication. Clear the path to return to the default Lua instance.

C++ can call `GetStateMachineAnimInstance()` on a Character, or cast the mesh component's `GetAnimInstance()` to `UStateMachineAnimInstance`, then use `SetFloat`, `SetBool`, `SetTrigger` and `ResetTrigger`. Actor Lua scripts can continue using `self:GetAnimInstance()` for the common parameter API.

An optional update script on the asset must define `UpdateAnimation(anim, dt)`. It receives the common parameter API and `GetMovementComponent()`; it cannot create states or players. See `Assets/Scripts/Animation/StateMachineUpdate.lua`. Declare the parameters used by the script in the asset first. Existing `CharacterAnim.lua` is for the original Lua-built graph and is not an asset-update script.

External writes occur before the optional Lua update; Lua wins if both write the same parameter. BlendSpace bindings are applied next, followed by state machine update and pose evaluation. Each character owns its playback state. Asset saves do not rebuild already-running characters; reapply the path or restart play to use the new definition.

## Boundaries

Each state contains one Sequence or one nonempty 1D BlendSpace. Transition conditions support Always, BoolEquals, FloatCompare, TriggerActive, SourceFinished, StateElapsedCompare, And, Or and Not. Lua callback conditions remain available in the original Lua-built instance only. Free-form AnimGraph, nested state machines and time scrubbing are not included.

Build validation does not run the editor or verify behavior in play mode.
