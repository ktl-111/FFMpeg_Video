package com.example.videolearn.utils;

import android.content.Intent;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;

import java.lang.ref.WeakReference;
import java.util.List;

/**
 * Created by steam_l on 2018/9/14.
 * Desprition :
 */

public class ResultUtils {
    private String tag_fragment = ResultFragment.class.getSimpleName();
    private static WeakReference<FragmentManager> mWeakReference;
    private int requestCode = 100;

    private ResultUtils() {
    }

    private static ResultUtils mInstance;

    public static ResultUtils getInstance(FragmentActivity activity) {
        mWeakReference = new WeakReference<FragmentManager>(activity.getSupportFragmentManager());
        return init();
    }

    public static ResultUtils getInstance(Fragment fragment) {
        mWeakReference = new WeakReference<FragmentManager>(fragment.getChildFragmentManager());
        return init();
    }

    private static ResultUtils init() {
        if (mInstance == null) {
            synchronized (ResultUtils.class) {
                if (mInstance == null) {
                    mInstance = new ResultUtils();
                }
            }
        }
        return mInstance;
    }

    public void request(Intent intent, int requestCode, RequestCallBack callBack) {
        FragmentManager manager = mWeakReference.get();
        if (manager != null) {
            getFragment(manager).startActivityForResult(intent, requestCode, callBack);
        }
    }

    public void permissions(String[] permissions, PermissionsCallBack callBack) {
        FragmentManager manager = mWeakReference.get();
        if (manager != null) {
            getFragment(manager).requestPermissions(permissions, requestCode, callBack);
        }
    }

    public void singlePermissions(String permissions, SinglePermissionsCallBack callBack) {
        FragmentManager manager = mWeakReference.get();
        if (manager != null) {
            getFragment(manager).requestSinglePermissions(permissions, requestCode, callBack);
        }
    }

    public ResultFragment getFragment(FragmentManager manager) {
        ResultFragment fragmentByTag = (ResultFragment) manager.findFragmentByTag(tag_fragment);
        if (fragmentByTag == null) {
            fragmentByTag = new ResultFragment();
            FragmentTransaction transaction = manager.beginTransaction();
            transaction.add(fragmentByTag, tag_fragment);
            transaction.commitNowAllowingStateLoss();
        }
        return fragmentByTag;
    }

    public interface RequestCallBack {
        void onActivityResult(int requestCode, int resultCode, Intent data);
    }

    /**
     * 多个请求的回调接口
     */
    public interface PermissionsCallBack {
        //        /**
        //         * 全部通过
        //         */
        //        void granted();

        /**
         * 未通过权限
         *
         * @param deniedList
         */
        void deniedList(List<String> deniedList);

        /**
         * 通过的权限
         *
         * @param grantedList
         */
        void grantedList(List<String> grantedList);
    }

    /**
     * 单个请求的回调接口
     */
    public interface SinglePermissionsCallBack {
        /**
         * @param granted 是否通过
         */
        void isGranted(boolean granted);
    }
}
