# State Machine Editor

## Create and connect states

1. In Content Drawer, right-click an Assets folder and choose **Create > Animation > State Machine**. Select the skeletal mesh and an `.animsm` save path under Assets.
2. Right-click the graph background and choose **Add State**. Select a Sequence or BlendSpace in the state Details. Declare Bool, Float and Trigger parameters under Parameters; bind each BlendSpace axis to a Float parameter.
3. Drag a state's **Out** pin to another state's **In** pin to create a transition. Dragging a pin onto empty space offers to create and connect a state. Entry selects the initial state; AnyState can transition into any state. Both are protected from deletion.
4. Each transition has its own small badge between two links. Click either link or the badge to select the transition. Details shows its endpoints, rule summary, blend time, priority, interrupt policy, restart flag and consumed triggers. Badges can be moved independently, including for reverse, parallel and self transitions.
5. Double-click the badge or either link, or choose **Edit Transition Rule**, to open its condition graph. Click **State Machine** in the breadcrumb to return.

Left-drag moves nodes or connects pins. Middle-drag pans; the wheel zooms. Frame All and Frame Selection focus the canvas. Right-click menus offer contextual actions. Select several nodes/links and press Delete to remove them. Removing a state also removes its transitions; disconnecting or removing the initial state requires reconnecting Entry. Ctrl+S saves. Node movement changes the asset; panning, zoom and selection do not.

## Edit a transition rule

Conditions are named nodes, not indices. Connect Bool outputs to inputs and finish at the protected **Can Enter Transition** node. For example:

```
[IsGrounded == true] -------\
                            AND --> [Can Enter Transition]
[AttackRequested active] ---/
```

Right-click empty space or drag a pin into empty space to add a compatible node. Supported nodes are Bool Constant, Bool Compare, Float Compare, Trigger Active, Source Finished, State Elapsed, AND, OR and NOT. Comparisons select a declared parameter from a type-filtered dropdown and compare it to the value inside the node. Separate Float value nodes and comparisons between two parameters are not included.

AND/OR have **+ Input** and **- Input** controls. NOT and the final output take one input. Connecting to an occupied input replaces its old connection; an output can feed several inputs. Cyclic rule links are rejected. Cyclic state transitions and self transitions are allowed.

New transitions start with **false -> Can Enter Transition**, so they cannot fire until configured. Missing required connections and invalid parameters are highlighted on a node and listed in Rule Errors. Click an error to open its rule. Invalid graphs block save and preview application. Unused nodes retain their layout but do not produce runtime conditions. Unused predicate nodes must still have valid parameter/value settings.

Parameter renames update rule references, BlendSpace bindings and consumed triggers inside the asset. Removing or changing the type of a referenced parameter is blocked with a list of its users. Lua script strings must be renamed manually.

## Preview and apply

**Apply to 3D Preview** opens Skeletal Mesh Viewer with a private snapshot of the working data, including unsaved edits. Play/Pause, Reset, Bool/Float edits and Trigger buttons control this instance. The state graph highlights the current state and the exact active transition. Apply again after editing. Lua updates are disabled in preview.

For a character, set its skeletal mesh component's **StateMachineAssetPath** to the `.animsm` file, for example `Assets/NewStateMachine.animsm`. The mesh must match. The path persists with scene serialization and duplication. Clear it to return to the default Lua instance.

C++ can use `GetStateMachineAnimInstance()` or cast the component's animation instance to `UStateMachineAnimInstance`. Actor Lua scripts can use `self:GetAnimInstance()` and `SetBool`, `SetFloat`, `SetTrigger` or `ResetTrigger`. An optional asset update script must provide `UpdateAnimation(anim, dt)`; see `Assets/Scripts/Animation/StateMachineUpdate.lua`.

## Compatibility and limits

Existing Version 1 `.animsm` files open without manual migration. Their conditions are converted into independent per-transition graphs in memory. Editing one transition no longer changes other transitions that previously shared its condition. Opening a file does not rewrite it; saving adds optional editor metadata and regenerates the existing runtime condition arrays. State IDs, state positions, transition order and behavior are preserved. No `NodeEditor.json` sidecar is created.

Each state still contains one Sequence or one nonempty 1D BlendSpace. Undo/Redo, copy/paste, a free-form AnimGraph, nested state machines, general Blueprint operations and live rebuilding of running characters are outside this version. Save does not change existing runtime snapshots; reapply or restart play to use edits.

Implementation verification covers Debug x64 compilation and linking only. The application and gameplay behavior are not run automatically.
