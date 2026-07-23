"""Validate Studio's legacy point preset and its shared Valve capsule data."""

import json
import math
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PRESET = Path(__file__).with_name("default_sas_visibility_points.json")
SOURCE = ROOT / "src" / "core" / "visibility_sampling.cpp"
VIEWER = Path(__file__).with_name("viewer.js")


def main() -> None:
    preset = json.loads(PRESET.read_text(encoding="utf-8"))
    points = preset["points"]
    assert preset.get("version") == 1
    assert preset.get("coordinate_space") == "source_local"
    assert preset.get("model") == "ctm_sas"
    assert preset["point_count"] == len(points) == 15
    assert all(isinstance(point.get("name"), str) and point["name"].strip() for point in points)
    assert len({point["name"] for point in points}) == len(points)
    assert all(math.isfinite(float(point[axis])) for point in points for axis in ("x", "y", "z"))

    viewer_text = VIEWER.read_text(encoding="utf-8")
    viewer_block = viewer_text.split("const k_runtime_body_bones = [", 1)[1].split("];", 1)[0]
    viewer_bones = re.findall(r'"([^"]+)"', viewer_block)
    assert len(viewer_bones) == len(points)
    assert "point_vec(points[index])" in viewer_text
    assert "point_vec(default_points[index])" not in viewer_text

    source_text = SOURCE.read_text(encoding="utf-8")
    source_block = source_text.split(
        "const std::array<visibility_capsule_binding, k_visibility_capsule_count> k_visibility_capsule_bindings {{",
        1,
    )[1].split("}};", 1)[0]
    number = r"[-+0-9.eE]+"
    source_capsules = []
    for name, start, end, radius in re.findall(
        rf'\{{"([^"]+)", \{{([^}}]+)\}}, \{{([^}}]+)\}}, ({number})f\}}', source_block
    ):
        parse = lambda values: tuple(float(value.removesuffix("f")) for value in values.split(", "))
        source_capsules.append((name, parse(start), parse(end), float(radius)))

    capsule_block = viewer_text.split("const k_valve_hitbox_capsules = [", 1)[1].split("];", 1)[0]
    viewer_capsules = []
    for name, start, end, radius in re.findall(
        rf'\["([^"]+)", \[([^]]+)\], \[([^]]+)\], ({number})\]', capsule_block
    ):
        parse = lambda values: tuple(float(value) for value in values.split(", "))
        viewer_capsules.append((name, parse(start), parse(end), float(radius)))

    assert len(source_capsules) == len(viewer_capsules) == 19
    for source, viewer in zip(source_capsules, viewer_capsules):
        assert source[0] == viewer[0]
        assert all(abs(left - right) < 1e-6 for left, right in zip(source[1], viewer[1]))
        assert all(abs(left - right) < 1e-6 for left, right in zip(source[2], viewer[2]))
        assert abs(source[3] - viewer[3]) < 1e-6

    print("Studio legacy points are valid and Valve capsules match compiled CS2FOW data")


if __name__ == "__main__":
    main()
