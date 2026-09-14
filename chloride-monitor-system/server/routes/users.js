const express = require("express");
const { createUser, deleteUser, listUsers, updateUser } = require("../services/auth-store");

const router = express.Router();

router.get("/", async (req, res, next) => {
  try {
    res.json({
      ok: true,
      users: await listUsers(),
    });
  } catch (error) {
    next(error);
  }
});

router.post("/", async (req, res, next) => {
  try {
    res.status(201).json({
      ok: true,
      user: await createUser(req.body || {}),
    });
  } catch (error) {
    next(error);
  }
});

router.patch("/:id", async (req, res, next) => {
  try {
    res.json({
      ok: true,
      user: await updateUser(req.params.id, req.body || {}, req.user),
    });
  } catch (error) {
    next(error);
  }
});

router.delete("/:id", async (req, res, next) => {
  try {
    res.json({
      ok: true,
      user: await deleteUser(req.params.id, req.user),
    });
  } catch (error) {
    next(error);
  }
});

module.exports = router;
