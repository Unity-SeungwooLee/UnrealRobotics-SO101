# -*- coding: utf-8 -*-
"""Regenerate the README architecture diagram (light + dark SVG).

Usage:  python docs/architecture.py
"""
import io, os

W, H = 1200, 700

FONT = "ui-sans-serif,-apple-system,'Segoe UI','Malgun Gothic','Apple SD Gothic Neo','Noto Sans KR',sans-serif"
MONO = "ui-monospace,SFMono-Regular,'Cascadia Mono',Consolas,'Courier New',monospace"

LIGHT = dict(
    bg="#ffffff", panel="#f6f8fa", panelStroke="#d8dee4",
    sub="#ffffff", card="#ffffff", cardStroke="#e1e4e8",
    text="#1f2328", muted="#59636e", line="#8c959f",
    blue="#0969da", green="#1a7f37", purple="#8250df",
    cyan="#0f7c8a", orange="#bc4c00", gray="#6e7781",
)
DARK = dict(
    bg="#0d1117", panel="#161b22", panelStroke="#30363d",
    sub="#0d1117", card="#161b22", cardStroke="#30363d",
    text="#e6edf3", muted="#9198a1", line="#6e7681",
    blue="#58a6ff", green="#3fb950", purple="#a371f7",
    cyan="#56d4dd", orange="#f0883e", gray="#8b949e",
)


def style(p):
    return """
  .bg{{fill:{bg}}}
  .panel{{fill:{panel};stroke:{panelStroke};stroke-width:1.5}}
  .sub{{fill:{sub};stroke:{panelStroke};stroke-width:1.5}}
  .card{{fill:{card};stroke:{cardStroke};stroke-width:1.5}}
  text{{font-family:{font};fill:{text}}}
  .badge{{font-size:13px;font-weight:700;letter-spacing:.02em}}
  .grp{{font-size:13.5px;font-weight:700}}
  .t{{font-size:13.5px;font-weight:700}}
  .s{{font-size:10.5px;fill:{muted}}}
  .b{{font-size:11px;fill:{text}}}
  .m{{font-family:{mono};font-size:10.5px;fill:{muted}}}
  .lbl{{font-size:10.5px;fill:{muted}}}
  .hd{{font-size:11px;font-weight:700}}
  .blue{{fill:{blue}}} .green{{fill:{green}}} .purple{{fill:{purple}}}
  .cyan{{fill:{cyan}}} .orange{{fill:{orange}}} .gray{{fill:{gray}}}
  .ln{{stroke:{line};stroke-width:1.6;fill:none}}
  .lnf{{fill:{line}}}
""".format(font=FONT, mono=MONO, **p)


def esc(s):
    return (s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;"))


class Svg(object):
    def __init__(self):
        self.o = io.StringIO()

    def w(self, s):
        self.o.write(u"  " + s + u"\n")

    def rect(self, x, y, w, h, cls, rx=10):
        self.w(u'<rect x="%g" y="%g" width="%g" height="%g" rx="%g" class="%s"/>' % (x, y, w, h, rx, cls))

    def text(self, x, y, s, cls="b", anchor="start"):
        a = u' text-anchor="%s"' % anchor if anchor != "start" else u""
        self.w(u'<text x="%g" y="%g" class="%s"%s>%s</text>' % (x, y, cls, a, esc(s)))

    def card(self, x, y, w, h, accent, title, subtitle, bullets):
        self.rect(x, y, w, h, "card", 9)
        self.w(u'<rect x="%g" y="%g" width="3" height="%g" rx="1.5" class="%s"/>'
               % (x + 1, y + 12, h - 24, accent))
        self.text(x + 16, y + 26, title, "t")
        if subtitle:
            self.text(x + 16, y + 44, subtitle, "s")
        by = y + 68
        for b in bullets:
            self.text(x + 16, by, b, "b")
            by += 19


def build(p):
    s = Svg()
    s.w(u'<rect width="%d" height="%d" class="bg"/>' % (W, H))

    # ---------------- Windows / Unreal ----------------
    s.rect(20, 20, 470, 640, "panel", 14)
    s.text(40, 46, u"Windows 11", "badge")

    s.rect(40, 62, 430, 578, "sub", 12)
    s.text(56, 86, u"Unreal Engine 5.4.4 · SO101_Twin", "grp")

    s.card(56, 100, 398, 152, "blue",
           u"URosBridgeSubsystem",
           u"GameInstanceSubsystem · WebSocket 단독 소유",
           [u"· Subscribe / Advertise / Publish",
            u"· 자동 재접속 (지수 백오프 1s → 30s)",
            u"· 재접속 시 구독 · 광고 목록 복원",
            u"· fragment 메시지 재조립"])

    s.card(56, 272, 398, 178, "purple",
           u"ARobotVisualizer",
           u"URDF 관절 시각화 · 로봇 상태 단일 소유",
           [u"· /joint_states → 7링크 6조인트 적용",
            u"· MoveIt 목표 publish (named / joint / pose)",
            u"· 녹화 · 재생 · E-Stop",
            u"· heartbeat 기반 연결 헬스 모니터링",
            u"· 관절 이력 링버퍼 (300 샘플 ≈ 10초)"])

    s.card(56, 470, 398, 150, "cyan",
           u"URobotControlWidget (UMG)",
           u"C++ 로직 + WBP 레이아웃 하이브리드",
           [u"· 연결 상태 점 5개 · 조작 버튼 · E-Stop",
            u"· 관절 한계 모니터 · 이력 그래프",
            u"· 녹화 목록 · 재생 진행률 · 토스트"])

    # ---------------- Link ----------------
    s.text(595, 192, u"WebSocket", "hd blue", "middle")
    s.text(595, 211, u"ws://127.0.0.1:9090/?x=1", "m", "middle")
    s.w(u'<line x1="502" y1="236" x2="688" y2="236" class="ln" '
        u'marker-start="url(#al)" marker-end="url(#ar)"/>')

    s.text(522, 296, u"ROS → UE", "hd green")
    for i, t in enumerate([u"/joint_states  30 Hz",
                           u"/robot_status",
                           u"/bridge_heartbeat  1 Hz"]):
        s.text(522, 318 + i * 18, t, "m")

    s.text(522, 402, u"UE → ROS", "hd blue")
    for i, t in enumerate([u"/robot_command",
                           u"/moveit_goal_named",
                           u"/moveit_goal_joints",
                           u"/moveit_goal_pose"]):
        s.text(522, 424 + i * 18, t, "m")

    # ---------------- WSL2 / ROS2 ----------------
    s.rect(700, 20, 480, 510, "panel", 14)
    s.text(720, 46, u"WSL2 · Ubuntu-22.04", "badge")
    s.text(720, 64, u"ROS2 humble + CycloneDDS", "s")

    s.card(720, 84, 440, 54, "blue",
           u"rosbridge_websocket", u"port 9090 · rosbridge v2 JSON", [])

    s.w(u'<line x1="744" y1="144" x2="744" y2="170" class="ln" '
        u'marker-start="url(#au)" marker-end="url(#ad)"/>')
    s.text(758, 161, u"ROS2 토픽 · CycloneDDS", "lbl")

    s.card(720, 176, 440, 92, "green",
           u"bridge_node", u"ROS2 ↔ worker 릴레이 (rclpy, Py3.10)",
           [u"· 명령 중계 · 상태 취합 · heartbeat 발행",
            u"· 녹화 목록 분할 발행 · 재생 진행률 5 Hz"])

    s.w(u'<line x1="744" y1="274" x2="744" y2="300" class="ln" '
        u'marker-start="url(#au)" marker-end="url(#ad)"/>')
    s.text(758, 291, u"ZeroMQ IPC", "lbl")

    s.card(720, 306, 440, 112, "green",
           u"lerobot_worker.py", u"conda: lerobot (Py3.12) · ABI 격리용 별도 프로세스",
           [u"· leader → follower 텔레오프",
            u"· 궤적 녹화 / 재생 (cosine ease-in-out)",
            u"· 이중 관절 클램핑 · 관절 한계 보고"])

    s.card(720, 436, 212, 62, "gray", u"robot_state_publisher", u"옵션 · RViz 표시용", [])
    s.card(948, 436, 212, 62, "gray", u"MoveIt 2 스택", u"옵션 · 플래닝 + Action server", [])

    # ---------------- Hardware ----------------
    s.w(u'<line x1="940" y1="534" x2="940" y2="586" class="ln" '
        u'marker-start="url(#au)" marker-end="url(#ad)"/>')
    s.text(954, 566, u"USB serial · usbipd-win", "lbl")

    s.rect(760, 590, 360, 70, "card", 9)
    s.w(u'<rect x="761" y="602" width="3" height="46" rx="1.5" class="orange"/>')
    s.text(776, 618, u"SO-ARM-101 (실물)", "t")
    s.text(776, 638, u"leader + follower · 6축 서보 암 · /dev/ttyACM*", "s")

    return s.o.getvalue()


def render(p):
    head = (u'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" '
            u'width="%d" height="%d" role="img" '
            u'aria-label="SO-ARM-101 digital twin architecture">\n' % (W, H, W, H))
    defs = u"""  <style>%s  </style>
  <defs>
    <marker id="ar" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M0 0 L10 5 L0 10 z" class="lnf"/>
    </marker>
    <marker id="al" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M0 0 L10 5 L0 10 z" class="lnf"/>
    </marker>
    <marker id="ad" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M0 0 L10 5 L0 10 z" class="lnf"/>
    </marker>
    <marker id="au" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse">
      <path d="M0 0 L10 5 L0 10 z" class="lnf"/>
    </marker>
  </defs>
""" % style(p)
    return head + defs + build(p) + u"</svg>\n"


dest = os.path.dirname(os.path.abspath(__file__))

for name, pal in (("architecture-light.svg", LIGHT), ("architecture-dark.svg", DARK)):
    path = os.path.join(dest, name)
    with io.open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(render(pal))
    print("wrote", path)
