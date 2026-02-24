// config.js - 統一設定與 Firebase 初始化
const ALLOSAURUS_API = "https://allosaurus-api-878665537417.asia-east1.run.app/recognize";

// Firebase 配置
const firebaseConfig = {
    apiKey: "AIzaSyD8oEe02SY2PCDe3d40NtTprrApqW_DhB8",
    authDomain: "test-5dbba.firebaseapp.com",
    projectId: "test-5dbba"
};

// 1. 初始化 Firebase (確保單例模式)
if (!firebase.apps.length) {
    firebase.initializeApp(firebaseConfig);
}

// 2. 匯出全域變數 (供各頁面使用)
const db = firebase.firestore();
const auth = firebase.auth();
const provider = new firebase.auth.GoogleAuthProvider();

/**
 * 3. 統一獲取設定的方法
 */
const getGlobalSettings = () => ({
    lang: localStorage.getItem('userLang') || 'en-US',
    key: localStorage.getItem('gptApiKey') || '',
    course: localStorage.getItem('userCourse') || 'all'
});

/**
 * 4. 共用登入/登出功能
 * 所有頁面呼叫此函式即可觸發 Google 登入
 */
const handleGoogleLogin = async () => {
    try {
        await auth.signInWithPopup(provider);
    } catch (error) {
        console.error("登入錯誤:", error);
        alert(`登入失敗: ${error.message}`);
    }
};

const handleLogout = async () => {
    try {
        await auth.signOut();
        // 登出後重新載入頁面以更新狀態
        window.location.reload();
    } catch (error) {
        console.error("登出錯誤:", error);
    }
};

// 監聽重定向結果 (若使用 redirect 模式)
auth.getRedirectResult().catch((error) => {
    if (error.code === 'auth/account-exists-with-different-credential') {
        alert("此帳號已存在不同的驗證方式");
    }
});

firebase.firestore().enablePersistence().catch(function(err) {
    console.error("無法啟用 Firebase 離線支援: ", err);
});