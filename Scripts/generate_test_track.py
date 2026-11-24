"""
generate_test_track.py  -  Grey-box TEST TRACK generator (Cosmic Hotshots, UE 5.7)

HOW TO RUN (inside Unreal Editor):
  1. Edit > Plugins: enable "Python Editor Script Plugin" (restart editor if asked).
  2. Open an EMPTY level (File > New Level > Basic or Empty).
  3. File > Execute Python Script...  and pick this file.
  Result: everything is spawned under the World Outliner folder "TestTrack".
  Running it again deletes the old "TestTrack" actors and rebuilds them.

WHAT IT BUILDS (all engine basic cubes, no assets needed):
  - Road: closed loop, ~30 m wide, with a smooth hill (hover/suspension test).
  - Walls on both sides (wall-bounce + scrape tests).
  - Checkpoints: TriggerBox actors tagged "Checkpoint" and "CP_<index>"
    (CP_0 is the start/finish line).
  - 8 PlayerStart actors on a 2x4 grid behind the start line.
  - Item boxes: 6 ItemBoxCluster actors, each a row of 3 boxes across the track.
    (Maps generated before the item system have plain cubes tagged "ItemBox" instead;
    the game hides those and puts real item boxes in their place when a race starts.)
  - Race line: one TrackSplineActor through the centre line (point 0 = start line).
    To add or refresh ONLY the race line in an existing map, run place_track_spline.py.
Track layout: long start straight with a hill right after the line, wide
sweeper, medium corner, S-kink, chicane, long left-hand return.

You can also run this file OUTSIDE Unreal (plain Python 3): it then only
writes test_track_centerline.csv (x,y,z in Unreal units) which Claude Code can
use to build a real spline-based track later.

NOTE: written against the UE 5.x Python API from knowledge, not tested in your
editor. If a call fails, paste the Output Log error into Claude Code.
"""
import math
import os

try:
    import unreal
except ImportError:  # running outside the editor
    unreal = None

# ------------------------------------------------------------------ settings
UU = 100.0                 # Unreal units per meter
TRACK_WIDTH_M = 30.0
WALL_HEIGHT_M = 4.0
WALL_THICK_M = 1.0
ROAD_THICK_M = 1.0
SAMPLE_STEP_M = 8.0        # distance between road segments
SEGMENT_OVERLAP = 1.2      # >1 hides gaps on curves
CHECKPOINT_EVERY_M = 250.0
GRID_ROWS = 4
GRID_ROW_SPACING_M = 14.0
GRID_LATERAL_M = 6.0
START_Z_M = 0.0
FOLDER = "TestTrack"

# item box placeholders (CTR style): clusters of boxes across the track width
ITEM_BOX_SPOTS = [0.14, 0.27, 0.42, 0.56, 0.72, 0.88]   # fraction of lap length
ITEM_BOX_LATERAL_M = [-9.0, 0.0, 9.0]                    # offsets across the track
ITEM_BOX_SIZE_M = 2.0
ITEM_BOX_HOVER_M = 2.0                                   # height of box center above road

# hills: (start as fraction of lap length from the start line, length in m, height in m)
HILLS = [(0.04, 140.0, 9.0)]

# closed loop control points in meters (x forward, y right). Start line = first point.
CTRL = [
    (300, 0), (600, 0),                    # start line at first point, straight ahead
    (790, 40), (900, 150), (930, 300),     # wide sweeper
    (870, 420), (740, 450),                # medium corner
    (640, 390), (560, 310), (450, 290),    # S-kink
    (370, 350), (300, 440), (180, 480),    # chicane
    (40, 470), (-80, 410), (-140, 290),    # long left-hand return
    (-140, 160), (-95, 55), (-10, 5),
    (150, 0),
]


# ------------------------------------------------------------ pure geometry
def _catmull(p0, p1, p2, p3, t):
    t2, t3 = t * t, t * t * t
    return tuple(
        0.5 * ((2 * p1[i]) + (-p0[i] + p2[i]) * t +
               (2 * p0[i] - 5 * p1[i] + 4 * p2[i] - p3[i]) * t2 +
               (-p0[i] + 3 * p1[i] - 3 * p2[i] + p3[i]) * t3)
        for i in range(2))


def build_centerline():
    """Returns list of (x_m, y_m, z_m) evenly spaced by SAMPLE_STEP_M, closed loop
    (last point is NOT a duplicate of the first), plus lap length in meters."""
    n = len(CTRL)
    dense = []
    for i in range(n):
        p0, p1, p2, p3 = CTRL[(i - 1) % n], CTRL[i], CTRL[(i + 1) % n], CTRL[(i + 2) % n]
        for k in range(60):
            dense.append(_catmull(p0, p1, p2, p3, k / 60.0))
    dense.append(dense[0])
    cum = [0.0]
    for a, b in zip(dense, dense[1:]):
        cum.append(cum[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    length = cum[-1]
    count = max(8, int(round(length / SAMPLE_STEP_M)))
    pts, j = [], 0
    for i in range(count):
        s = length * i / count
        while cum[j + 1] < s:
            j += 1
        f = (s - cum[j]) / max(1e-9, cum[j + 1] - cum[j])
        x = dense[j][0] + (dense[j + 1][0] - dense[j][0]) * f
        y = dense[j][1] + (dense[j + 1][1] - dense[j][1]) * f
        pts.append([x, y, 0.0])
    # elevation
    for i, p in enumerate(pts):
        s = length * i / count
        z = START_Z_M
        for frac, hl, hh in HILLS:
            s0 = frac * length
            if s0 <= s <= s0 + hl:
                z += hh * 0.5 * (1.0 - math.cos(2.0 * math.pi * (s - s0) / hl))
        p[2] = z
    return [tuple(p) for p in pts], length


def sample_at(pts, length, s):
    """Position (x,y,z) and yaw (deg) at arc length s (wraps around the lap)."""
    n = len(pts)
    step = length / n
    s = s % length
    i = int(s // step) % n
    f = (s - i * step) / step
    a, b = pts[i], pts[(i + 1) % n]
    pos = tuple(a[k] + (b[k] - a[k]) * f for k in range(3))
    yaw = math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
    return pos, yaw


def export_csv(path, pts):
    with open(path, "w") as f:
        f.write("x_uu,y_uu,z_uu\n")
        for x, y, z in pts:
            f.write("%.1f,%.1f,%.1f\n" % (x * UU, y * UU, z * UU))


# ------------------------------------------------------------- Unreal part
def _spawn_box(sub, mesh, loc_m, yaw, pitch, size_m, label, folder):
    loc = unreal.Vector(loc_m[0] * UU, loc_m[1] * UU, loc_m[2] * UU)
    rot = unreal.Rotator(roll=0.0, pitch=pitch, yaw=yaw)
    a = sub.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot)
    a.set_actor_scale3d(unreal.Vector(size_m[0] * UU / 100.0,
                                      size_m[1] * UU / 100.0,
                                      size_m[2] * UU / 100.0))
    a.set_actor_label(label)
    a.set_folder_path(folder)
    comp = a.static_mesh_component
    comp.set_static_mesh(mesh)
    try:
        comp.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    except Exception:
        pass
    return a


def _clear_old(sub):
    for a in sub.get_all_level_actors():
        try:
            if str(a.get_folder_path()).startswith(FOLDER):
                sub.destroy_actor(a)
        except Exception:
            pass


def build_in_unreal():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    mesh = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube.Cube")
    pts, length = build_centerline()
    n = len(pts)
    step = length / n
    unreal.log("TestTrack: lap length %.0f m, %d segments" % (length, n))

    with unreal.ScopedEditorTransaction("Generate test track"):
        _clear_old(sub)

        for i in range(n):
            a, b = pts[i], pts[(i + 1) % n]
            dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
            flat = math.hypot(dx, dy)
            seg_len = math.sqrt(flat * flat + dz * dz) * SEGMENT_OVERLAP
            yaw = math.degrees(math.atan2(dy, dx))
            pitch = math.degrees(math.atan2(dz, flat))
            mid = ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2)
            nx, ny = -math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
            lift = 0.003 * (i % 2)  # tiny offset against z-fighting

            _spawn_box(sub, mesh, (mid[0], mid[1], mid[2] - ROAD_THICK_M / 2 + lift),
                       yaw, pitch, (seg_len, TRACK_WIDTH_M, ROAD_THICK_M),
                       "Road_%03d" % i, FOLDER + "/Road")

            off = TRACK_WIDTH_M / 2 + WALL_THICK_M / 2
            for side, nm in ((1, "L"), (-1, "R")):
                _spawn_box(sub, mesh,
                           (mid[0] + nx * off * side, mid[1] + ny * off * side,
                            mid[2] + WALL_HEIGHT_M / 2 - ROAD_THICK_M / 2),
                           yaw, pitch, (seg_len, WALL_THICK_M, WALL_HEIGHT_M),
                           "Wall%s_%03d" % (nm, i), FOLDER + "/Walls")

        # start / finish marker (thin strip across the road)
        pos, yaw = sample_at(pts, length, 0.0)
        _spawn_box(sub, mesh, (pos[0], pos[1], pos[2] + 0.02), yaw, 0.0,
                   (2.0, TRACK_WIDTH_M, 0.05), "StartFinishLine", FOLDER)

        # checkpoints
        cp_count = max(4, int(round(length / CHECKPOINT_EVERY_M)))
        for k in range(cp_count):
            pos, yaw = sample_at(pts, length, length * k / cp_count)
            loc = unreal.Vector(pos[0] * UU, pos[1] * UU, (pos[2] + 6.0) * UU)
            rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw)
            t = sub.spawn_actor_from_class(unreal.TriggerBox, loc, rot)
            t.set_actor_label("Checkpoint_%02d" % k)
            t.set_folder_path(FOLDER + "/Checkpoints")
            t.set_editor_property("tags", [unreal.Name("Checkpoint"),
                                           unreal.Name("CP_%d" % k)])
            try:  # half extents in UU: 3 m along track, full width, 6 m high
                box = t.get_component_by_class(unreal.BoxComponent)
                box.set_box_extent(unreal.Vector(3.0 * UU, (TRACK_WIDTH_M / 2 + 1.0) * UU,
                                                 6.0 * UU), True)
            except Exception as e:
                unreal.log_warning("Could not resize checkpoint %d: %s" % (k, e))

        # grid: slot 1 = front-left, slot 2 = front-right, slot 3 = 2nd row ...
        for slot in range(8):
            row, side = slot // 2, (1 if slot % 2 == 0 else -1)
            s = -(8.0 + row * GRID_ROW_SPACING_M)
            pos, yaw = sample_at(pts, length, s)
            nx, ny = -math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
            loc = unreal.Vector((pos[0] + nx * GRID_LATERAL_M * side) * UU,
                                (pos[1] + ny * GRID_LATERAL_M * side) * UU,
                                (pos[2] + 2.0) * UU)
            ps = sub.spawn_actor_from_class(unreal.PlayerStart, loc,
                                            unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            ps.set_actor_label("GridSlot_%d" % (slot + 1))
            ps.set_folder_path(FOLDER + "/Grid")

        # item boxes: one ItemBoxCluster actor per spot; it spawns its row of boxes when the race starts
        for ci, frac in enumerate(ITEM_BOX_SPOTS):
            pos, yaw = sample_at(pts, length, length * frac)
            loc = unreal.Vector(pos[0] * UU, pos[1] * UU, (pos[2] + ITEM_BOX_HOVER_M) * UU)
            cluster = sub.spawn_actor_from_class(unreal.ItemBoxCluster, loc,
                                                 unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            cluster.set_actor_label("ItemBoxCluster_%02d" % ci)
            cluster.set_folder_path(FOLDER + "/ItemBoxes")
            cluster.set_editor_property("lateral_offsets", [lat * UU for lat in ITEM_BOX_LATERAL_M])

        place_track_spline(sub, pts)

    unreal.log("TestTrack: done. Check the World Outliner folder '%s'." % FOLDER)


def place_track_spline(sub, pts):
    """Replaces the race line actor (ATrackSplineActor) with one through the centre line points.
    Spline point 0 = start/finish line, points in driving order, closed loop. Touches nothing else."""
    for a in sub.get_all_level_actors():
        if isinstance(a, unreal.TrackSplineActor):
            sub.destroy_actor(a)

    first = unreal.Vector(pts[0][0] * UU, pts[0][1] * UU, pts[0][2] * UU)
    actor = sub.spawn_actor_from_class(unreal.TrackSplineActor, first, unreal.Rotator(0.0, 0.0, 0.0))
    actor.set_actor_label("TrackSpline")
    actor.set_folder_path(FOLDER + "/Race")
    spline = actor.get_component_by_class(unreal.SplineComponent)
    spline.clear_spline_points(False)
    for x, y, z in pts:
        spline.add_spline_point(unreal.Vector(x * UU, y * UU, z * UU),
                                unreal.SplineCoordinateSpace.WORLD, False)
    spline.set_closed_loop(True, False)
    spline.update_spline()
    unreal.log("TestTrack: track spline with %d points, length %.0f m"
               % (spline.get_number_of_spline_points(), spline.get_spline_length() / UU))
    return actor


if __name__ == "__main__":
    if unreal is not None:
        build_in_unreal()
    else:
        pts, length = build_centerline()
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           "test_track_centerline.csv")
        export_csv(out, pts)
        print("Lap length: %.0f m, %d points. Wrote %s" % (length, len(pts), out))
