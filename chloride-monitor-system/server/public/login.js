const form = document.getElementById("loginForm");
const loginBtn = document.getElementById("loginBtn");
const loginMessage = document.getElementById("loginMessage");
const forgotForm = document.getElementById("forgotForm");
const forgotBtn = document.getElementById("forgotBtn");
const forgotMessage = document.getElementById("forgotMessage");

async function checkExistingSession() {
  try {
    const response = await fetch("/api/auth/me");
    if (response.ok) {
      window.location.replace("/");
    }
  } catch {
    // Stay on login page.
  }
}

form.addEventListener("submit", async (event) => {
  event.preventDefault();
  loginBtn.disabled = true;
  loginMessage.textContent = "正在登录...";
  loginMessage.classList.remove("failed");

  try {
    const response = await fetch("/api/auth/login", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        username: form.username.value.trim(),
        password: form.password.value,
      }),
    });

    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(data.message || `HTTP ${response.status}`);
    }

    window.location.replace("/");
  } catch (error) {
    loginMessage.textContent = error.message;
    loginMessage.classList.add("failed");
  } finally {
    loginBtn.disabled = false;
  }
});

forgotForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  forgotBtn.disabled = true;
  forgotMessage.textContent = "正在重置密码...";
  forgotMessage.classList.remove("failed");

  try {
    const response = await fetch("/api/auth/forgot-password", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({
        username: forgotForm.username.value.trim(),
        password: forgotForm.password.value,
      }),
    });

    const data = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(data.message || `HTTP ${response.status}`);
    }

    forgotForm.reset();
    forgotMessage.textContent = "密码已重置，请使用新密码登录";
  } catch (error) {
    forgotMessage.textContent = error.message;
    forgotMessage.classList.add("failed");
  } finally {
    forgotBtn.disabled = false;
  }
});

checkExistingSession();
