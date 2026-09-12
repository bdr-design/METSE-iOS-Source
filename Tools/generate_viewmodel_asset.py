#!/usr/bin/env python3
"""Generate METSE's original engineering viewmodel mesh.

The generated OBJ is an in-repository, reviewable bridge asset for the native
Metal mesh path. It is deliberately replaceable by a licensed production asset
without changing renderer code.
"""

from __future__ import annotations

from math import cos, pi, sin, sqrt
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Content" / "Assets" / "Weapons" / "M4A1"


class Obj:
    def __init__(self) -> None:
        self.vertices: list[tuple[float, float, float]] = []
        self.normals: list[tuple[float, float, float]] = []
        self.faces: list[tuple[str, list[tuple[int, int]]]] = []

    def _face(self, material: str, points: list[tuple[tuple[float, float, float], tuple[float, float, float]]]) -> None:
        refs = []
        for position, normal in points:
            self.vertices.append(position)
            self.normals.append(normal)
            refs.append((len(self.vertices), len(self.normals)))
        self.faces.append((material, refs))

    def box(self, name: str, center: tuple[float, float, float], size: tuple[float, float, float], material: str, bevel: float = 0.0) -> None:
        cx, cy, cz = center
        sx, sy, sz = (v * 0.5 for v in size)
        # Bevel is encoded as stepped end caps so receiver edges catch light.
        if bevel > 0 and min(sx, sy, sz) > bevel:
            self.box(name + "_core", center, (size[0] - 2 * bevel, size[1], size[2]), material)
            self.box(name + "_cap_l", (cx - sx + bevel * 0.5, cy, cz), (bevel, size[1] - bevel, size[2] - bevel), material)
            self.box(name + "_cap_r", (cx + sx - bevel * 0.5, cy, cz), (bevel, size[1] - bevel, size[2] - bevel), material)
            return
        corners = [
            (cx - sx, cy - sy, cz - sz), (cx + sx, cy - sy, cz - sz),
            (cx + sx, cy + sy, cz - sz), (cx - sx, cy + sy, cz - sz),
            (cx - sx, cy - sy, cz + sz), (cx + sx, cy - sy, cz + sz),
            (cx + sx, cy + sy, cz + sz), (cx - sx, cy + sy, cz + sz),
        ]
        for ids, normal in [
            ((0, 3, 2, 1), (0, 0, -1)), ((4, 5, 6, 7), (0, 0, 1)),
            ((0, 4, 7, 3), (-1, 0, 0)), ((1, 2, 6, 5), (1, 0, 0)),
            ((0, 1, 5, 4), (0, -1, 0)), ((3, 7, 6, 2), (0, 1, 0)),
        ]:
            self._face(material, [(corners[i], normal) for i in ids])

    def cylinder_z(self, name: str, center: tuple[float, float, float], radius: float, length: float, material: str, segments: int = 20) -> None:
        cx, cy, cz = center
        za, zb = cz - length * 0.5, cz + length * 0.5
        for i in range(segments):
            a0, a1 = 2 * pi * i / segments, 2 * pi * (i + 1) / segments
            p0, p1 = (cx + radius * cos(a0), cy + radius * sin(a0), za), (cx + radius * cos(a1), cy + radius * sin(a1), za)
            p2, p3 = (p1[0], p1[1], zb), (p0[0], p0[1], zb)
            n0, n1 = (cos(a0), sin(a0), 0), (cos(a1), sin(a1), 0)
            self._face(material, [(p0, n0), (p1, n1), (p2, n1), (p3, n0)])
            self._face(material, [((cx, cy, za), (0, 0, -1)), (p1, (0, 0, -1)), (p0, (0, 0, -1))])
            self._face(material, [((cx, cy, zb), (0, 0, 1)), (p3, (0, 0, 1)), (p2, (0, 0, 1))])

    def cylinder_between(self, name: str, start: tuple[float, float, float], end: tuple[float, float, float], radius: float, material: str, segments: int = 16) -> None:
        ax, ay, az = start
        bx, by, bz = end
        dx, dy, dz = bx - ax, by - ay, bz - az
        length = sqrt(dx * dx + dy * dy + dz * dz)
        w = (dx / length, dy / length, dz / length)
        reference = (0.0, 1.0, 0.0) if abs(w[1]) < 0.9 else (1.0, 0.0, 0.0)
        ux, uy, uz = (reference[1] * w[2] - reference[2] * w[1], reference[2] * w[0] - reference[0] * w[2], reference[0] * w[1] - reference[1] * w[0])
        ul = sqrt(ux * ux + uy * uy + uz * uz)
        u = (ux / ul, uy / ul, uz / ul)
        v = (w[1] * u[2] - w[2] * u[1], w[2] * u[0] - w[0] * u[2], w[0] * u[1] - w[1] * u[0])
        rings = []
        for base in (start, end):
            ring = []
            for i in range(segments):
                a = 2 * pi * i / segments
                n = tuple(u[j] * cos(a) + v[j] * sin(a) for j in range(3))
                ring.append((tuple(base[j] + radius * n[j] for j in range(3)), n))
            rings.append(ring)
        for i in range(segments):
            j = (i + 1) % segments
            self._face(material, [rings[0][i], rings[0][j], rings[1][j], rings[1][i]])

    def write(self, path: Path) -> None:
        lines = ["# METSE original engineering viewmodel", "mtllib viewmodel.mtl", "o METSE_M4A1_Viewmodel"]
        lines.extend(f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in self.vertices)
        lines.extend(f"vn {x:.6f} {y:.6f} {z:.6f}" for x, y, z in self.normals)
        current = None
        for material, refs in self.faces:
            if material != current:
                lines.append(f"usemtl {material}")
                current = material
            lines.append("f " + " ".join(f"{v}//{n}" for v, n in refs))
        path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build() -> Obj:
    o = Obj()
    # Upper/lower receiver and controls.
    o.box("upper", (0.0, 0.0, 0.02), (0.17, 0.16, 0.36), "gunmetal", 0.012)
    o.box("lower", (0.0, -0.105, 0.06), (0.15, 0.08, 0.28), "receiver", 0.01)
    o.box("charging_handle", (0.0, 0.09, 0.17), (0.12, 0.025, 0.09), "gunmetal")
    o.cylinder_z("forward_assist", (0.095, 0.015, 0.10), 0.022, 0.055, "control", 14)
    o.cylinder_z("selector", (-0.088, -0.08, 0.06), 0.018, 0.018, "control", 14)
    # Magazine, grip and trigger guard.
    o.box("magazine", (0.0, -0.22, -0.015), (0.115, 0.27, 0.14), "magazine", 0.008)
    o.box("pistol_grip", (0.0, -0.235, 0.165), (0.10, 0.25, 0.105), "polymer", 0.01)
    o.box("trigger_guard", (0.0, -0.105, 0.195), (0.10, 0.018, 0.10), "gunmetal")
    # Buffer tube and adjustable stock.
    o.cylinder_z("buffer", (0.0, 0.015, 0.34), 0.035, 0.34, "gunmetal", 18)
    o.box("stock_body", (0.0, -0.005, 0.54), (0.18, 0.17, 0.28), "polymer", 0.016)
    o.box("stock_pad", (0.0, -0.01, 0.70), (0.19, 0.20, 0.055), "rubber", 0.008)
    # Free-float handguard and Picatinny rails.
    o.cylinder_z("handguard", (0.0, 0.005, -0.36), 0.092, 0.42, "handguard", 20)
    for y, x in ((0.104, 0.0), (-0.104, 0.0), (0.0, 0.104), (0.0, -0.104)):
        o.box("rail", (x, y, -0.36), (0.025 if x else 0.15, 0.025 if y else 0.15, 0.44), "rail")
    for z in [i * -0.045 - 0.17 for i in range(9)]:
        o.box("rail_slot", (0.0, 0.122, z), (0.14, 0.018, 0.014), "edge")
    # Barrel, gas block, muzzle device and suppressor.
    o.cylinder_z("barrel", (0.0, 0.005, -0.69), 0.024, 0.30, "barrel", 20)
    o.box("gas_block", (0.0, 0.015, -0.56), (0.095, 0.105, 0.08), "gunmetal")
    o.cylinder_z("suppressor", (0.0, 0.005, -0.91), 0.052, 0.36, "suppressor", 24)
    o.cylinder_z("muzzle_ring", (0.0, 0.005, -1.105), 0.057, 0.028, "edge", 24)
    # EOTech-like holographic sight, protective hood and glass.
    o.box("optic_base", (0.0, 0.13, 0.00), (0.13, 0.035, 0.18), "optic")
    o.box("optic_left", (-0.058, 0.205, -0.02), (0.022, 0.15, 0.12), "optic")
    o.box("optic_right", (0.058, 0.205, -0.02), (0.022, 0.15, 0.12), "optic")
    o.box("optic_top", (0.0, 0.275, -0.02), (0.135, 0.022, 0.12), "optic")
    o.box("optic_glass", (0.0, 0.21, -0.078), (0.095, 0.105, 0.008), "glass")
    # PEQ box, pressure switch and vertical foregrip.
    o.box("peq", (-0.085, 0.13, -0.36), (0.10, 0.07, 0.18), "polymer", 0.008)
    o.cylinder_z("peq_laser", (-0.085, 0.15, -0.47), 0.018, 0.035, "glass", 14)
    o.box("pressure_switch", (0.055, 0.13, -0.34), (0.045, 0.018, 0.13), "rubber")
    o.box("foregrip", (0.0, -0.145, -0.39), (0.075, 0.22, 0.08), "polymer", 0.009)
    # Gloved hands and sleeves establish a believable first-person connection.
    o.cylinder_between("support_forearm", (-0.28, -0.42, 0.10), (-0.05, -0.20, -0.38), 0.075, "sleeve", 18)
    o.cylinder_between("support_hand", (-0.05, -0.20, -0.38), (0.0, -0.13, -0.40), 0.065, "glove", 18)
    o.cylinder_between("firing_forearm", (0.34, -0.42, 0.25), (0.09, -0.23, 0.15), 0.078, "sleeve", 18)
    o.cylinder_between("firing_hand", (0.09, -0.23, 0.15), (0.02, -0.15, 0.13), 0.066, "glove", 18)
    return o


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    mesh = build()
    mesh.write(OUT / "viewmodel.obj")
    (OUT / "viewmodel.mtl").write_text("""# METSE original material palette
newmtl gunmetal
Kd 0.075 0.085 0.080
newmtl receiver
Kd 0.105 0.115 0.105
newmtl control
Kd 0.12 0.13 0.12
newmtl magazine
Kd 0.10 0.11 0.095
newmtl polymer
Kd 0.055 0.060 0.050
newmtl rubber
Kd 0.025 0.028 0.025
newmtl handguard
Kd 0.115 0.105 0.080
newmtl rail
Kd 0.06 0.065 0.06
newmtl edge
Kd 0.16 0.17 0.15
newmtl barrel
Kd 0.08 0.085 0.08
newmtl suppressor
Kd 0.12 0.115 0.095
newmtl optic
Kd 0.055 0.06 0.055
newmtl glass
Kd 0.06 0.28 0.26
newmtl sleeve
Kd 0.18 0.20 0.13
newmtl glove
Kd 0.14 0.13 0.10
""", encoding="utf-8")
    print(f"generated {len(mesh.vertices)} vertices and {len(mesh.faces)} faces")


if __name__ == "__main__":
    main()
