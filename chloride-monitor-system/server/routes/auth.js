const express = require("express");
const {
  authenticateUser,
  resetForgottenPassword,
  updateOwnProfile,
} = require("../services/auth-store");
const { clearSession, createSession, requireLogin } = require("../middleware/auth");

const router = express.Router();

router.post("/login", async (req, res, next) => {
  try {
    const user = await authenticateUser(req.body?.username, req.body?.password);
    createSession(res, user);
    res.json({ ok: true, user });
  } catch (error) {
    next(error);
  }
});

router.get("/me", requireLogin, (req, res) => {
  res.json({ ok: true, user: req.user });
});

router.patch("/me", requireLogin, async (req, res, next) => {
  try {
    const user = await updateOwnProfile(req.user, req.body || {});
    createSession(res, user);
    res.json({ ok: true, user });
  } catch (error) {
    next(error);
  }
});

router.post("/forgot-password", async (req, res, next) => {
  try {
    await resetForgottenPassword(req.body || {});
    res.json({ ok: true });
  } catch (error) {
    next(error);
  }
});

router.post("/logout", (req, res) => {
  clearSession(req, res);
  res.json({ ok: true });
});

module.exports = router;
